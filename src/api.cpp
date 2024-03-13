#include "api.h"
#include "dynamicObject.h"
#include "glm/fwd.hpp"
#include "logger.h"
#include "world.h"

#include <GLFW/glfw3.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <zmq/zmq.hpp>
#undef Status
#include "protos/api.pb.h"

using namespace std;

int BatchedRequest::nextId = 0;

Api::Api(std::string bindAddress, WorldInterface* world): world(world) {
  context = zmq::context_t(2);
  logger = make_shared<spdlog::logger>("Api", fileSink);
  commandServer = new ProtobufCommandServer(this, bindAddress, context);
  offRenderThread = thread(&Api::poll, this);
}


CommandServer::CommandServer(Api* api, std::string bindAddress, zmq::context_t& context): api(api) {
  logger = make_shared<spdlog::logger>("CommandServer", fileSink);
  logger->set_level(spdlog::level::info);
  socket = zmq::socket_t(context, zmq::socket_type::rep);
  socket.bind (bindAddress);
}

void Api::ProtobufCommandServer::poll(WorldInterface *world) {
  try {
    zmq::message_t recv;
    zmq::recv_result_t result = socket.recv(recv);

    zmq::message_t reply(5);
    memcpy(reply.data(), "recv", 5);

    if (result >= 0) {
      ApiRequest apiRequest;
      apiRequest.ParseFromArray(recv.data(), recv.size());

      api->grabBatched();
      auto batchedRequests = api->getBatchedRequests();
      auto request = BatchedRequest(apiRequest);
      batchedRequests->push(request);
      api->releaseBatched();


      socket.send(reply, zmq::send_flags::none);
    }
  } catch (zmq::error_t &e) {}
}

void Api::poll() {
  while (continuePolling) {
    if (commandServer != NULL) {
      commandServer->poll(world);
    }
  }
}

void Api::mutateWorld() {
  long time = glfwGetTime();
  long target = time + 0.005;
  grabBatched();
  auto batchedRequests = getBatchedRequests();
  for (; time <= target && batchedRequests->size() != 0; time = glfwGetTime()) {
    // Handle a request
    batchedRequests->pop();
  }
  releaseBatched();
}

void Api::grabBatched() {
  renderMutex.lock();
}

void Api::releaseBatched() { renderMutex.unlock(); }

queue<BatchedRequest> *Api::getBatchedRequests() {
  return &batchedRequests;
}

Api::~Api() {
  continuePolling = false;
  context.shutdown();
  offRenderThread.join();
  delete commandServer;
}
