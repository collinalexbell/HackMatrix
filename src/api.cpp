#include "api.h"
#include "controls.h"
#include "dynamicObject.h"
#include "glm/fwd.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>
#include "logger.h"
#include "renderer.h"
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
#include <zmq/zmq.hpp>
#include <unordered_set>
#undef Status
#include "protos/api.pb.h"

using namespace std;

int BatchedRequest::nextId = 0;

Api::Api(std::string bindAddress,
         shared_ptr<EntityRegistry> registry,
         Controls* controls,
         Renderer* renderer,
         World* world,
         shared_ptr<WindowManager::WindowManager> wm)
  : registry(registry)
  , controls(controls)
  , renderer(renderer)
  , world(world)
  , wm(wm)
{
  context = zmq::context_t(2);
  logger = make_shared<spdlog::logger>("Api", fileSink);
  logger->set_level(spdlog::level::debug);
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
      batchedRequests->push(request);
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
  long time = glfwGetTime();
  long target = time + 0.005;
  grabBatched();
  auto batchedRequests = getBatchedRequests();
  for (; time <= target && batchedRequests->size() != 0; time = glfwGetTime()) {
    processBatchedRequest(batchedRequests->front());
    batchedRequests->pop();
  }
  releaseBatched();
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
