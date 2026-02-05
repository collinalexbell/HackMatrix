#include "api.h"
#include "controls.h"
#include "dynamicObject.h"
#include "glm/fwd.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include "logger.h"
#include "renderer.h"
#include "model.h"
#include "systems/KeyAndLock.h"
#include "systems/Move.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <spdlog/common.h>
#include <sstream>
#include <string>
#include <thread>
#include <zmq/zmq.hpp>
#include <unordered_set>
#include "time_utils.h"
#undef Status
#include "protos/api.pb.h"

extern "C" {
#include <wayland-server-core.h>
}

using namespace std;

namespace {
void
log_to_tmp_api(const std::string& msg)
{
  const char* path = std::getenv("MATRIX_WLROOTS_OUTPUT");
  if (!path) {
    path = "/tmp/matrix-wlroots-output.log";
  }
  FILE* f = std::fopen(path, "a");
  if (!f) {
    return;
  }
  std::fwrite(msg.c_str(), 1, msg.size(), f);
  std::fclose(f);
}

glm::vec3
toVec3(const Vector& v)
{
  return glm::vec3(v.x(), v.y(), v.z());
}

} // namespace

int BatchedRequest::nextId = 0;

EngineStatus
Api::buildStatus() const
{
  EngineStatus status;
  uint32_t totalEntities = 0;
  if (registry) {
    auto view = registry->view<entt::entity>();
    totalEntities = static_cast<uint32_t>(view.size_hint());
    auto wlView = registry->view<WaylandApp::Component>();
    status.set_wayland_apps(static_cast<uint32_t>(wlView.size()));
  }
  status.set_total_entities(totalEntities);
  bool waylandFocus = false;
  if (wm && registry) {
    if (auto focused = wm->getCurrentlyFocusedApp()) {
      if (registry->all_of<WaylandApp::Component>(*focused)) {
        waylandFocus = true;
      }
    }
  }
  status.set_wayland_focus(waylandFocus);
  // Debug: dump registry counts to /tmp when requested.
  FILE* f = std::fopen("/tmp/matrix-wlroots-output.log", "a");
  if (f) {
    std::fprintf(f,
                 "status: total=%u wayland=%u focus=%d registry_ptr=%p wm_ptr=%p\n",
                 totalEntities,
                 status.wayland_apps(),
                 status.wayland_focus() ? 1 : 0,
                 registry.get(),
                 wm.get());
    std::fclose(f);
  }
  if (renderer) {
    if (auto* camera = renderer->getCamera()) {
      auto* pos = status.mutable_camera_position();
      pos->set_x(camera->position.x);
      pos->set_y(camera->position.y);
      pos->set_z(camera->position.z);
    }
  }
  return status;
}

void
Api::updateCachedStatus()
{
  auto newStatus = buildStatus();
  {
    char buf[128];
    snprintf(buf,
             sizeof(buf),
             "cachedStatus update wayland=%u total=%u\n",
             newStatus.wayland_apps(),
             newStatus.total_entities());
    log_to_tmp_api(std::string(buf));
  }
  std::lock_guard<std::mutex> lk(statusMutex);
  cachedStatus = newStatus;
  // Fulfill any pending STATUS requests.
  for (auto& pending : pendingStatus) {
    if (!pending) {
      continue;
    }
    pending->response.Clear();
    pending->response.set_requestid(pending->requestId);
    pending->response.set_success(true);
    *pending->response.mutable_status() = cachedStatus;
    pending->ready = true;
  }
  statusCv.notify_all();
}

Api::Api(std::string bindAddress,
         shared_ptr<EntityRegistry> registry,
         Controls* controls,
         Renderer* renderer,
         World* world,
         WindowManager::WindowManagerPtr wm)
  : registry(registry)
  , controls(controls)
  , renderer(renderer)
  , world(world)
  , wm(wm)
{
  context = zmq::context_t(2);
  logger = make_shared<spdlog::logger>("Api", fileSink);
  logger->set_level(spdlog::level::debug);
  {
    char buf[256];
    snprintf(buf, sizeof(buf), "api: binding command server at %s\n", bindAddress.c_str());
    log_to_tmp_api(std::string(buf));
  }
  try {
    commandServer = new ProtobufCommandServer(this, bindAddress, context);
  } catch (const std::exception& e) {
    logger->error("Failed to bind API socket on {}: {}", bindAddress, e.what());
    throw;
  }
  offRenderThread = thread(&Api::poll, this);
}

CommandServer::CommandServer(Api* api,
                             std::string bindAddress,
                             zmq::context_t& context)
  : api(api)
{
  logger = make_shared<spdlog::logger>("CommandServer", fileSink);
  logger->set_level(spdlog::level::info);
  socket = zmq::socket_t(context, zmq::socket_type::rep);
  socket.bind(bindAddress);
}

void
Api::ProtobufCommandServer::poll()
{
  try {
    zmq::message_t recv;
    zmq::recv_result_t result = socket.recv(recv);

    if (result >= 0) {
      ApiRequest apiRequest;
      apiRequest.ParseFromArray(recv.data(), recv.size());

      api->grabBatched();
      auto batchedRequests = api->getBatchedRequests();
      auto request = BatchedRequest(apiRequest);
      if (apiRequest.type() == CLEAR_VOXELS) {
        request.actionId = api->allocateActionId();
      }
      if (apiRequest.type() == QUIT) {
        log_to_tmp_api("api quit requested\n");
        // Process QUIT immediately so the display is terminated even if the
        // main mutate loop isn't ticking (e.g., in headless tests).
        api->processBatchedRequest(request);
      } else if (apiRequest.type() == STATUS) {
        auto pending = std::make_shared<PendingStatusRequest>();
        pending->requestId = request.id;
        {
          std::lock_guard<std::mutex> lk(api->statusMutex);
          api->pendingStatus.push_back(pending);
        }
        api->releaseBatched();
        // Wait for render thread to fill response
        std::unique_lock<std::mutex> lk(api->statusMutex);
        api->statusCv.wait_for(lk, std::chrono::milliseconds(2000), [&pending]() {
          return pending->ready;
        });
        ApiRequestResponse response = pending->response;
        {
          char buf[128];
          snprintf(buf,
                   sizeof(buf),
                   "api status reply id=%ld wayland=%u total=%u ready=%d\n",
                   (long)request.id,
                   response.status().wayland_apps(),
                   response.status().total_entities(),
                   pending->ready ? 1 : 0);
          log_to_tmp_api(std::string(buf));
        }
        std::string serializedResponse;
        response.SerializeToString(&serializedResponse);
        zmq::message_t reply(serializedResponse.size());
        memcpy(reply.data(), serializedResponse.c_str(), serializedResponse.size());
        socket.send(reply, zmq::send_flags::none);
        return;
      } else {
        batchedRequests->push(request);
      }
      api->releaseBatched();

      ApiRequestResponse response;
      response.set_requestid(request.id);
      if (request.actionId.has_value()) {
        response.set_actionid(request.actionId.value());
      }
      response.set_success(true);

      // Serialize the protocol buffer object to a byte array
      std::string serializedResponse;
      response.SerializeToString(&serializedResponse);

      // Create a zmq::message_t object from the serialized data
      zmq::message_t reply(serializedResponse.size());
      memcpy(
        reply.data(), serializedResponse.c_str(), serializedResponse.size());

      socket.send(reply, zmq::send_flags::none);
    }
  } catch (zmq::error_t& e) {
  }
}

void
Api::poll()
{
  while (continuePolling) {
    if (commandServer != NULL) {
      commandServer->poll();
    }
  }
}

void
Api::processBatchedRequest(BatchedRequest batchedRequest)
{
  auto entityId = (entt::entity)batchedRequest.request.entityid();
  switch (batchedRequest.request.type()) {
    case MOVE: {
      auto move = batchedRequest.request.move();
      systems::translate(registry,
                         entityId,
                         glm::vec3(move.xdelta(), move.ydelta(), move.zdelta()),
                         move.unitspersecond());
      break;
    }
    case TURN_KEY: {
      auto turnKey = batchedRequest.request.turnkey();
      if (turnKey.on()) {
        systems::turnKey(registry, entityId);
      } else {
        systems::unturnKey(registry, entityId);
      }
      break;
    }
    case PLAYER_MOVE: {
      auto playerMove = batchedRequest.request.playermove();
      glm::vec3 pos(playerMove.position().x(),
                    playerMove.position().y(),
                    playerMove.position().z());
      glm::vec3 rotation(playerMove.rotation().x(),
                         playerMove.rotation().y(),
                         playerMove.rotation().z());
      controls->moveTo(pos, rotation, playerMove.unitspersecond());
      break;
    }
    case UNFOCUS_WINDOW: {
      if (wm) {
        wm->unfocusApp();
      }
      break;
    }
    case ADD_VOXELS: {
      auto voxels = batchedRequest.request.addvoxels();
      std::vector<glm::vec3> positions;
      positions.reserve(voxels.voxels_size());
      for (const auto& v : voxels.voxels()) {
        positions.emplace_back(v.x(), v.y(), v.z());
      }
      float size = voxels.size() > 0 ? voxels.size() : 1.0f;
      bool replace = voxels.replace();
      glm::vec3 color(1.0f);
      if (voxels.has_color()) {
        color = glm::vec3(voxels.color().x(), voxels.color().y(), voxels.color().z());
      }
      // Allow empty positions when replace is true so callers can clear voxels.
      bool shouldUpdate = replace || !positions.empty();
      if (renderer != nullptr && shouldUpdate) {
        if (logger) {
          logger->info("AddVoxels: count={}, replace={}, size={}",
                       positions.size(),
                       replace,
                       size);
        }
        std::cout << "[API] AddVoxels count=" << positions.size()
                  << " replace=" << replace << " size=" << size
                  << " color=(" << color.x << "," << color.y << "," << color.z
                  << ")" << std::endl;
        renderer->setLines(world != nullptr ? world->getLines()
                                            : std::vector<Line>{});
        renderer->addVoxels(positions, replace, size, color);
      }
      break;
    }
    case CLEAR_VOXELS: {
      auto clear = batchedRequest.request.clearvoxels();
      glm::vec3 min(clear.x().min(), clear.y().min(), clear.z().min());
      glm::vec3 max(clear.x().max(), clear.y().max(), clear.z().max());
      if (logger) {
        logger->info("ClearVoxels request: min=({}, {}, {}), max=({}, {}, {})",
                     min.x,
                     min.y,
                     min.z,
                     max.x,
                     max.y,
                     max.z);
      }
      std::cout << "[API] ClearVoxels request min=(" << min.x << ", " << min.y
                << ", " << min.z << ") max=(" << max.x << ", " << max.y << ", "
                << max.z << ")" << std::endl;
      registerClearArea(min, max, batchedRequest.actionId);
      break;
    }
    case CONFIRM_ACTION: {
      auto confirm = batchedRequest.request.confirmaction();
      confirmClearArea(confirm.actionid());
      break;
    }
    case QUIT: {
      // Allow external harnesses/tests to terminate the compositor cleanly.
      if (logger) {
        logger->info("Received QUIT request; terminating display");
      }
      log_to_tmp_api("api quit requested\n");
      if (display) {
        wl_display_terminate(display);
      }
      break;
    }
    case KEY_REPLAY: {
      if (wm) {
        std::vector<std::pair<std::string, uint32_t>> entries;
        const auto& replay = batchedRequest.request.keyreplay();
        entries.reserve(replay.entries_size());
        for (const auto& e : replay.entries()) {
          entries.emplace_back(e.sym(), e.delay_ms());
        }
        wm->keyReplay(entries);
      }
      break;
    }
    case POINTER_REPLAY: {
      if (controls) {
        const auto& replay = batchedRequest.request.pointerreplay();
        uint64_t cumulative = 0;
        for (const auto& e : replay.entries()) {
          cumulative += e.delay_ms();
          // Deliver immediately; delay handling could be extended later if needed.
          bool handled = controls->handlePointerButton(e.button(), e.pressed());
          {
            char buf[160];
            snprintf(buf,
                     sizeof(buf),
                     "api pointer replay button=%u pressed=%d handled=%d cumulative_ms=%llu\n",
                     e.button(),
                     e.pressed() ? 1 : 0,
                     handled ? 1 : 0,
                     (unsigned long long)cumulative);
            log_to_tmp_api(std::string(buf));
          }
        }
      }
      break;
    }
    case ADD_COMPONENT: {
      const auto& add = batchedRequest.request.addcomponent();
      const auto& component = add.component();
      entt::entity target =
        static_cast<entt::entity>(batchedRequest.request.entityid());
      if (!registry) {
        break;
      }
      switch (component.type()) {
        case COMPONENT_TYPE_POSITIONABLE: {
          if (!component.has_positionable()) {
            break;
          }
          const auto& data = component.positionable();
          glm::vec3 pos = toVec3(data.position());
          glm::vec3 origin =
            data.has_origin() ? toVec3(data.origin()) : glm::vec3(0.0f);
          glm::vec3 rotation = toVec3(data.rotation());
          float scale = data.scale();
          if (registry->any_of<Positionable>(target)) {
            auto& positionable = registry->get<Positionable>(target);
            positionable.pos = pos;
            positionable.origin = origin;
            positionable.rotate = rotation;
            positionable.scale = scale;
            positionable.damage();
          } else {
            registry->emplace<Positionable>(
              target, pos, origin, rotation, scale);
          }
          break;
        }
        case COMPONENT_TYPE_MODEL: {
          if (!component.has_model()) {
            break;
          }
          const auto& data = component.model();
          if (data.model_path().empty()) {
            break;
          }
          if (registry->any_of<Model>(target)) {
            registry->removePersistent<Model>(target);
          }
          registry->emplace<Model>(target, data.model_path());
          break;
        }
        default:
          break;
      }
      break;
    }
    case DELETE_COMPONENT: {
      const auto& del = batchedRequest.request.deletecomponent();
      entt::entity target =
        static_cast<entt::entity>(batchedRequest.request.entityid());
      if (!registry) {
        break;
      }
      switch (del.component_type()) {
        case COMPONENT_TYPE_POSITIONABLE:
          if (registry->any_of<Positionable>(target)) {
            registry->removePersistent<Positionable>(target);
          }
          break;
        case COMPONENT_TYPE_MODEL:
          if (registry->any_of<Model>(target)) {
            registry->removePersistent<Model>(target);
          }
          break;
        default:
          break;
      }
      break;
    }
    case EDIT_COMPONENT: {
      const auto& edit = batchedRequest.request.editcomponent();
      const auto& component = edit.component();
      entt::entity target =
        static_cast<entt::entity>(batchedRequest.request.entityid());
      if (!registry) {
        break;
      }
      switch (component.type()) {
        case COMPONENT_TYPE_POSITIONABLE: {
          if (!component.has_positionable() ||
              !registry->any_of<Positionable>(target)) {
            break;
          }
          const auto& data = component.positionable();
          auto& positionable = registry->get<Positionable>(target);
          positionable.pos = toVec3(data.position());
          positionable.origin =
            data.has_origin() ? toVec3(data.origin()) : glm::vec3(0.0f);
          positionable.rotate = toVec3(data.rotation());
          positionable.scale = data.scale();
          positionable.damage();
          break;
        }
        case COMPONENT_TYPE_MODEL: {
          if (!component.has_model() || !registry->any_of<Model>(target)) {
            break;
          }
          const auto& data = component.model();
          if (data.model_path().empty()) {
            break;
          }
          if (registry->any_of<Model>(target)) {
            registry->removePersistent<Model>(target);
          }
          registry->emplace<Model>(target, data.model_path());
          break;
        }
        default:
          break;
      }
      break;
    }
    case STATUS: {
      // STATUS requests are handled synchronously in poll().
      break;
    }
    default:
      break;
  }
}

std::vector<Line>
Api::buildClearAreaLines(const glm::vec3& min, const glm::vec3& max) const
{
  glm::vec3 lower(std::min(min.x, max.x),
                  std::min(min.y, max.y),
                  std::min(min.z, max.z));
  glm::vec3 upper(std::max(min.x, max.x),
                  std::max(min.y, max.y),
                  std::max(min.z, max.z));
  const glm::vec3 color(1.0f, 0.3f, 0.2f);

  auto makeLine = [&](glm::vec3 a, glm::vec3 b) {
    Line l;
    l.points[0] = a;
    l.points[1] = b;
    l.color = color;
    return l;
  };

  glm::vec3 c000(lower.x, lower.y, lower.z);
  glm::vec3 c100(upper.x, lower.y, lower.z);
  glm::vec3 c010(lower.x, upper.y, lower.z);
  glm::vec3 c110(upper.x, upper.y, lower.z);
  glm::vec3 c001(lower.x, lower.y, upper.z);
  glm::vec3 c101(upper.x, lower.y, upper.z);
  glm::vec3 c011(lower.x, upper.y, upper.z);
  glm::vec3 c111(upper.x, upper.y, upper.z);

  std::vector<Line> lines;
  lines.reserve(12);
  // Bottom
  lines.emplace_back(makeLine(c000, c100));
  lines.emplace_back(makeLine(c100, c110));
  lines.emplace_back(makeLine(c110, c010));
  lines.emplace_back(makeLine(c010, c000));
  // Top
  lines.emplace_back(makeLine(c001, c101));
  lines.emplace_back(makeLine(c101, c111));
  lines.emplace_back(makeLine(c111, c011));
  lines.emplace_back(makeLine(c011, c001));
  // Vertical edges
  lines.emplace_back(makeLine(c000, c001));
  lines.emplace_back(makeLine(c100, c101));
  lines.emplace_back(makeLine(c110, c111));
  lines.emplace_back(makeLine(c010, c011));

  return lines;
}

std::vector<glm::vec3>
Api::buildClearAreaVoxels(const glm::vec3& min, const glm::vec3& max) const
{
  glm::vec3 lower(std::min(min.x, max.x),
                  std::min(min.y, max.y),
                  std::min(min.z, max.z));
  glm::vec3 upper(std::max(min.x, max.x),
                  std::max(min.y, max.y),
                  std::max(min.z, max.z));
  float s = renderer != nullptr ? renderer->getVoxelSize() : 1.0f;
  if (s <= 0.0f) {
    s = 1.0f;
  }
  std::unordered_set<glm::vec3> voxels;
  auto addEdge = [&](glm::vec3 fixedA, glm::vec3 fixedB, bool varyX, bool varyY, bool varyZ) {
    if (varyX) {
      for (float x = lower.x; x <= upper.x + 0.0001f; x += s) {
        voxels.insert(glm::vec3(x, fixedA.y, fixedA.z));
        voxels.insert(glm::vec3(x, fixedB.y, fixedB.z));
      }
    } else if (varyY) {
      for (float y = lower.y; y <= upper.y + 0.0001f; y += s) {
        voxels.insert(glm::vec3(fixedA.x, y, fixedA.z));
        voxels.insert(glm::vec3(fixedB.x, y, fixedB.z));
      }
    } else if (varyZ) {
      for (float z = lower.z; z <= upper.z + 0.0001f; z += s) {
        voxels.insert(glm::vec3(fixedA.x, fixedA.y, z));
        voxels.insert(glm::vec3(fixedB.x, fixedB.y, z));
      }
    }
  };

  // Edges parallel to X
  addEdge(glm::vec3(lower.x, lower.y, lower.z),
          glm::vec3(lower.x, upper.y, lower.z),
          true,
          false,
          false);
  addEdge(glm::vec3(lower.x, lower.y, upper.z),
          glm::vec3(lower.x, upper.y, upper.z),
          true,
          false,
          false);

  // Edges parallel to Y
  addEdge(glm::vec3(lower.x, lower.y, lower.z),
          glm::vec3(upper.x, lower.y, lower.z),
          false,
          true,
          false);
  addEdge(glm::vec3(lower.x, lower.y, upper.z),
          glm::vec3(upper.x, lower.y, upper.z),
          false,
          true,
          false);

  // Edges parallel to Z
  addEdge(glm::vec3(lower.x, lower.y, lower.z),
          glm::vec3(lower.x, lower.y, upper.z),
          false,
          false,
          true);
  addEdge(glm::vec3(upper.x, lower.y, lower.z),
          glm::vec3(upper.x, lower.y, upper.z),
          false,
          false,
          true);

  // Opposite edges on top face for Y/Z and X/Z
  addEdge(glm::vec3(lower.x, upper.y, lower.z),
          glm::vec3(lower.x, upper.y, upper.z),
          false,
          false,
          true);
  addEdge(glm::vec3(upper.x, upper.y, lower.z),
          glm::vec3(upper.x, upper.y, upper.z),
          false,
          false,
          true);

  addEdge(glm::vec3(lower.x, lower.y, lower.z),
          glm::vec3(lower.x, upper.y, lower.z),
          false,
          true,
          false);
  addEdge(glm::vec3(upper.x, lower.y, lower.z),
          glm::vec3(upper.x, upper.y, lower.z),
          false,
          true,
          false);

  addEdge(glm::vec3(lower.x, lower.y, upper.z),
          glm::vec3(lower.x, upper.y, upper.z),
          false,
          true,
          false);
  addEdge(glm::vec3(upper.x, lower.y, upper.z),
          glm::vec3(upper.x, upper.y, upper.z),
          false,
          true,
          false);
  return std::vector<glm::vec3>(voxels.begin(), voxels.end());
}

int64_t
Api::registerClearArea(const glm::vec3& min,
                       const glm::vec3& max,
                       std::optional<int64_t> requestedId)
{
  int64_t actionId = requestedId.has_value() ? requestedId.value()
                                             : allocateActionId();
  glm::vec3 lower(std::min(min.x, max.x),
                  std::min(min.y, max.y),
                  std::min(min.z, max.z));
  glm::vec3 upper(std::max(min.x, max.x),
                  std::max(min.y, max.y),
                  std::max(min.z, max.z));
  pendingClearAreas[actionId] =
    ClearAreaAction{ actionId, lower, upper, {}, {} };
  // Build a voxel outline so it is visible even if line rendering is disabled.
  auto previewVoxels = buildClearAreaVoxels(lower, upper);
  pendingClearAreas[actionId].previewVoxels = previewVoxels;
  if (renderer != nullptr && !previewVoxels.empty()) {
    renderer->addVoxels(
      previewVoxels, false, renderer->getVoxelSize(), glm::vec3(1.0f, 0.2f, 0.2f));
  }
  if (logger) {
    logger->info("Registered clear area {}: min=({}, {}, {}), max=({}, {}, {})",
                 actionId,
                 lower.x,
                 lower.y,
                 lower.z,
                 upper.x,
                 upper.y,
                 upper.z);
  }
  std::cout << "[API] Registered clear area id=" << actionId
            << " min=(" << lower.x << ", " << lower.y << ", " << lower.z
            << ") max=(" << upper.x << ", " << upper.y << ", " << upper.z
            << ")" << std::endl;
  return actionId;
}

bool
Api::confirmClearArea(int64_t actionId)
{
  auto it = pendingClearAreas.find(actionId);
  if (it == pendingClearAreas.end()) {
    return false;
  }
  const auto& action = it->second;
  if (renderer != nullptr) {
    renderer->clearVoxelsInBox(action.min, action.max);
  }
  if (world != nullptr) {
    // No lines used anymore; voxel outline is cleared by the clear box itself.
  }
  if (logger) {
    logger->info("Confirmed clear area {}", actionId);
  }
  std::cout << "[API] Confirmed clear area id=" << actionId << std::endl;
  pendingClearAreas.erase(it);
  return true;
}

void
Api::mutateEntities()
{
  long time = nowSeconds();
  long target = time + 0.005;
  grabBatched();
  auto batchedRequests = getBatchedRequests();
  for (; time <= target && batchedRequests->size() != 0; time = nowSeconds()) {
    processBatchedRequest(batchedRequests->front());
    batchedRequests->pop();
  }
  releaseBatched();
  updateCachedStatus();
}

void
Api::grabBatched()
{
  renderMutex.lock();
}

void
Api::releaseBatched()
{
  renderMutex.unlock();
}

queue<BatchedRequest>*
Api::getBatchedRequests()
{
  return &batchedRequests;
}

Api::~Api()
{
  continuePolling = false;
  context.shutdown();
  offRenderThread.join();
  delete commandServer;
}
