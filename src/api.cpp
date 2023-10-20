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
        case ADD_CUBE: {
            const AddCube &cubeToAdd = apiRequest.addcube();
            api->grabBatched();
            auto batchedCubes = api->getBatchedCubes();
            glm::vec3 pos(cubeToAdd.x(), cubeToAdd.y(), cubeToAdd.z());
            batchedCubes->push(Cube{pos, cubeToAdd.blocktype()});
            api->releaseBatched();
            break;
        }
        case CLEAR_BOX: {
          const ClearBox &boxToClear = apiRequest.clearbox();
          api->grabBatched();
          auto batchedCubes = api->getBatchedCubes();

          bool x1Smaller = boxToClear.x1() < boxToClear.x2();
          float x1 = x1Smaller ? boxToClear.x1() : boxToClear.x2();
          float x2 = x1Smaller ? boxToClear.x2() : boxToClear.x1();

          bool y1Smaller = boxToClear.y1() < boxToClear.y2();
          float y1 = y1Smaller ? boxToClear.y1() : boxToClear.y2();
          float y2 = y1Smaller ? boxToClear.y2() : boxToClear.y1();

          bool z1Smaller = boxToClear.z1() < boxToClear.z2();
          float z1 = z1Smaller ? boxToClear.z1() : boxToClear.z2();
          float z2 = z1Smaller ? boxToClear.z2() : boxToClear.z1();

          stringstream boxString;
          boxString << "clearBox: "
                    << x1 << ", " << y1 << ", " << z1 << ", "
                    << x2 << ", " << y2 << ", " << z2 << endl;


          logger->critical(boxString.str());
          logger->flush();
          for(int x = x1; x <= x2; x++) {
            for(int y = y1; y <= y2; y++) {
              for(int z = z1; z <= z2; z++) {
                glm::vec3 pos(x,y,z);
                batchedCubes->push(Cube{pos, -1});
              }
            }
          }
          api->releaseBatched();
          break;
        }
        case ADD_LINE: {
          const AddLine &lineToAdd = apiRequest.addline();
          api->grabBatched();
          auto batchedLines = api->getBatchedLines();
          glm::vec3 pos1(lineToAdd.x1(), lineToAdd.y1(), lineToAdd.z1());
          glm::vec3 pos2(lineToAdd.x2(), lineToAdd.y2(), lineToAdd.z2());
          glm::vec3 color(0.5, 0.5, 0.5);
          batchedLines->push(Line{{pos1, pos2}, color});
          api->releaseBatched();
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
    std::string data(static_cast<char *>(request.data()), request.size());

    std::istringstream iss(data);

    while (!iss.eof()) {
      api->grabBatched();
      auto cubesToAdd = api->getBatchedCubes();
      std::string command;
      int type;
      int x, y, z;
      iss >> command >> x >> y >> z >> type;

      stringstream ss;
      ss << command << "," << x << "," << y << "," << z << endl;

      if (command == "c") {
        glm::vec3 pos(x, y, z);
        cubesToAdd->push(Cube{pos, type});
      }
      api->releaseBatched();
    }

    //  Send reply back to client
    zmq::message_t reply(5);
    memcpy(reply.data(), "recv", 5);
    socket.send(reply, zmq::send_flags::none);
  }
}

void Api::pollFor() {
  while (continuePolling) {
    if (legacyCommandServer != NULL) {
      legacyCommandServer->legacyPollForWorldCommands(world);
    }
    if (commandServer != NULL) {
      commandServer->pollForWorldCommands(world);
    }
  }
}

void Api::mutateWorld() {
  long time = glfwGetTime();
  long target = time + 0.10;
  bool hasDelete = false;
  grabBatched();
  stringstream logStream;
  for (; time <= target && batchedCubes.size() != 0; time = glfwGetTime()) {
    Cube c = batchedCubes.front();
    if(c.blockType == -1) {
      hasDelete = true;
    }
    world->addCube(c);
    batchedCubes.pop();
  }
  time = glfwGetTime();
  target = time + 0.10;
  for (; time <= target && batchedLines.size() != 0; time = glfwGetTime()) {
    Line l = batchedLines.front();
    if (l.color.r < 0) {
      hasDelete = true;
    }
    world->addLine(l);
    batchedLines.pop();
  }
  if(hasDelete) {
    world->refreshRenderer();
  }
  releaseBatched();
}

void Api::grabBatched() {
  renderMutex.lock();
}

void Api::releaseBatched() { renderMutex.unlock(); }

queue<Cube> *Api::getBatchedCubes(){
  return &batchedCubes;
}

queue<Line> *Api::getBatchedLines() {
  return &batchedLines;
}

Api::~Api() {
  continuePolling = false;
  offRenderThread.join();
  delete legacyCommandServer;
  delete commandServer;
}
