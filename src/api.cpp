#include "api.h"
#include "controls.h"
#include "dynamicObject.h"
#include "glm/fwd.hpp"
#include "logger.h"
#include "renderer.h"
#include "systems/KeyAndLock.h"
#include "systems/Move.h"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <spdlog/common.h>
#include <sstream>
#include <string>
#include <zmq/zmq.hpp>
#undef Status
#include "protos/api.pb.h"

using namespace std;

int BatchedRequest::nextId = 0;

Api::Api(std::string bindAddress,
         shared_ptr<EntityRegistry> registry,
         Controls* controls,
         Renderer* renderer,
         shared_ptr<WindowManager::WindowManager> wm)
  : registry(registry)
  , controls(controls)
  , renderer(renderer)
  , wm(wm)
{
  context = zmq::context_t(2);
  logger = make_shared<spdlog::logger>("Api", fileSink);
  logger->set_level(spdlog::level::debug);
  commandServer = new ProtobufCommandServer(this, bindAddress, context);
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
      batchedRequests->push(request);
      api->releaseBatched();

      ApiRequestResponse response;
      response.set_requestid(request.id);

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
      // Allow empty positions when replace is true so callers can clear voxels.
      bool shouldUpdate = replace || !positions.empty();
      if (renderer != nullptr && shouldUpdate) {
        renderer->addVoxels(positions, replace, size);
      }
      break;
    }
    default:
      break;
  }
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
