#include "api.h"
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

Api::Api(std::string bindAddress, World* world): world(world) {
  context = zmq::context_t(2);
  logger = make_shared<spdlog::logger>("Api", fileSink);
  legacyCommandServer = new CommandServer(this, "tcp://*:5555", context);
  commandServer = new CommandServer(this, bindAddress, context);
  offRenderThread = thread(&Api::pollFor, this);
}


CommandServer::CommandServer(Api* api, std::string bindAddress, zmq::context_t& context): api(api) {
  logger = make_shared<spdlog::logger>("CommandServer", fileSink);
  socket = zmq::socket_t(context, zmq::socket_type::rep);
  socket.bind (bindAddress);
}

void Api::requestWorldData(World* world, string serverAddr) {
  zmq::message_t reply;
  zmq::message_t request(5);
  zmq::context_t clientContext = zmq::context_t(2);
  zmq::socket_t clientSocket = zmq::socket_t(clientContext, zmq::socket_type::req);
  try {
    clientSocket.connect(serverAddr);
    memcpy (request.data (), "init", 5);
    clientSocket.send(request, zmq::send_flags::none);
    zmq::recv_result_t result = clientSocket.recv(reply, zmq::recv_flags::dontwait);
  } catch(...) {
    logger->critical("couldn't connect to the init server");
  }
}

void CommandServer::pollForWorldCommands(World *world) {
    zmq::message_t recv;
    zmq::recv_result_t result = socket.recv(recv, zmq::recv_flags::dontwait);

    if (result >= 0) {
        ApiRequest apiRequest;
        apiRequest.ParseFromArray(recv.data(), recv.size());

        switch(apiRequest.type()) {
        case ADD_CUBE:
          {
            const AddCube &cubeToAdd = apiRequest.addcube();
            auto batchedCubes = api->grabBatchedCubes();
            glm::vec3 pos(cubeToAdd.x(), cubeToAdd.y(), cubeToAdd.z());
            batchedCubes->push(Cube{pos, cubeToAdd.blocktype()});
            api->releaseBatchedCubes();
            break;
          }
        case CLEAR_BOX: {
          const ClearBox &boxToClear = apiRequest.clearbox();
          break;
        }
        default:
          break;
        }

        // Send a reply back to the client
        zmq::message_t reply(5);
        memcpy(reply.data(), "recv", 5);
        socket.send(reply, zmq::send_flags::none);
    }
}

void CommandServer::legacyPollForWorldCommands(World *world) {
  zmq::message_t request;

  zmq::recv_result_t result = socket.recv(request, zmq::recv_flags::dontwait);
  if (result >= 0) {
    logger->critical("result");
    std::string data(static_cast<char *>(request.data()), request.size());

    std::istringstream iss(data);

    while (!iss.eof()) {
      auto cubesToAdd = api->grabBatchedCubes();
      std::string command;
      int type;
      int x, y, z;
      iss >> command >> x >> y >> z >> type;

      stringstream ss;
      ss << command << "," << x << "," << y << "," << z << endl;
      logger->critical(ss.str());

      if (command == "c") {
        glm::vec3 pos(x, y, z);
        cubesToAdd->push(Cube{pos, type});
      }
      api->releaseBatchedCubes();
    }

    //  Send reply back to client
    zmq::message_t reply(5);
    memcpy(reply.data(), "recv", 5);
    socket.send(reply, zmq::send_flags::none);
  }
}

void Api::pollFor() {
  logger->critical("entering pollFor");
  while (continuePolling) {
    if (legacyCommandServer != NULL) {
      legacyCommandServer->legacyPollForWorldCommands(world);
    }
    if (commandServer != NULL) {
      commandServer->pollForWorldCommands(world);
    }
  }
  logger->critical("exiting pollFor");
}

void Api::mutateWorld() {
  long time = glfwGetTime();
  long target = time + 0.10;
  auto cubesToAdd = grabBatchedCubes();
  stringstream logStream;
  for (; time <= target && cubesToAdd->size() != 0; time = glfwGetTime()) {
    Cube c = cubesToAdd->front();
    logStream << "addingCube:" << c.position.x << "," << c.position.y << ","
              << c.position.z << endl;
    logger->critical(logStream.str());
    world->addCube(c);
    cubesToAdd->pop();
  }
  releaseBatchedCubes();
}

queue<Cube> *Api::grabBatchedCubes() {
  renderMutex.lock();
  return &batchedCubes;
}

void Api::releaseBatchedCubes() { renderMutex.unlock(); }

Api::~Api() {
  continuePolling = false;
  offRenderThread.join();
  delete legacyCommandServer;
  delete commandServer;
}
