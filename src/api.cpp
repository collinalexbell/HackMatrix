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

void Api::requestWorldData(WorldInterface* world, string serverAddr) {
  zmq::message_t reply;
  zmq::message_t request(5);
  zmq::context_t clientContext = zmq::context_t(2);
  zmq::socket_t clientSocket = zmq::socket_t(clientContext, zmq::socket_type::req);
  try {
    clientSocket.connect(serverAddr);
    memcpy (request.data (), "init", 5);
    auto err = clientSocket.send(request, zmq::send_flags::dontwait);
    if(err != -1) {
      zmq::recv_result_t result = clientSocket.recv(reply, zmq::recv_flags::dontwait);
    }
  } catch(...) {
    logger->critical("couldn't connect to the init server");
  }
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

      switch (apiRequest.type()) {
      case ADD_CUBE: {
        const AddCube &cubeToAdd = apiRequest.addcube();
        api->grabBatched();
        auto batchedCubes = api->getBatchedCubes();
        batchedCubes->push(ApiCube{cubeToAdd.x(), cubeToAdd.y(), cubeToAdd.z(),
                                   cubeToAdd.blocktype()});
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
        boxString << "clearBox: " << x1 << ", " << y1 << ", " << z1 << ", "
                  << x2 << ", " << y2 << ", " << z2 << endl;

        logger->critical(boxString.str());
        logger->flush();
        for (int x = x1; x <= x2; x++) {
          for (int y = y1; y <= y2; y++) {
            for (int z = z1; z <= z2; z++) {
              glm::vec3 pos(x, y, z);
              batchedCubes->push(ApiCube{(float)x, (float)y, (float)z, -1});
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
        float intensity = lineToAdd.intensity();
        glm::vec3 color(intensity, intensity, intensity);
        batchedLines->push(Line{{pos1, pos2}, color});
        api->releaseBatched();
        break;
      }
      case ADD_CUBES: {
        const AddCubes &cubesToAdd = apiRequest.addcubes();
        auto cubes = cubesToAdd.cubes();
        logger->debug("adding " + to_string(cubes.size()) + "cubes");
        api->grabBatched();
        auto batchedCubes = api->getBatchedCubes();
        for (int i = 0; i < cubes.size(); i++) {
          glm::vec3 pos(cubes[i].x(), cubes[i].y(), cubes[i].z());
          batchedCubes->push(ApiCube{cubes[i].x(), cubes[i].y(), cubes[i].z(),
                                     cubes[i].blocktype()});
        }
        api->releaseBatched();
        break;
      }
      case MOVE: {
        // I need to add the command to a dynamic objects queue that can
        // then be processed on main thread.
        Move movement = apiRequest.move();
        auto id = movement.id();
        auto delta = glm::vec3(movement.xdelta(), movement.ydelta(), movement.zdelta());
        auto obj = world->getDynamicObjects()->getObjectById(id);
        if(obj != NULL) {
          obj->move(delta);
        }
        break;
      }
      case GET_IDS: {
        world->getDynamicObjects()->getObjectIds();
        string stringifiedIds;
        reply = zmq::message_t(stringifiedIds.length());
        memcpy(reply.data(), stringifiedIds.data(), stringifiedIds.length());
        break;
      }
      default:
        break;
      }

      socket.send(reply, zmq::send_flags::none);
    }
  } catch (zmq::error_t &e) {}
}

void Api::TextCommandServer::poll(WorldInterface *world) {
  try {
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
          cubesToAdd->push(ApiCube{(float)x,(float)y,(float)z,type});
        }
        api->releaseBatched();
      }

      //  Send reply back to client
      zmq::message_t reply(5);
      memcpy(reply.data(), "recv", 5);
      socket.send(reply, zmq::send_flags::none);
    }
  } catch(zmq::error_t& e) {}
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
  bool hasMeshChange = false;
  grabBatched();
  stringstream logStream;
  for (; time <= target && batchedCubes.size() != 0; time = glfwGetTime()) {
    ApiCube c = batchedCubes.front();
    world->addCube(c.x, c.y, c.z, c.blockType);
    batchedCubes.pop();
    hasMeshChange = true;
  }
  time = glfwGetTime();
  target = time + 0.10;
  for (; time <= target && batchedLines.size() != 0; time = glfwGetTime()) {
    Line l = batchedLines.front();
    world->addLine(l);
    batchedLines.pop();
  }
  releaseBatched();
  if(hasMeshChange) {
    world->mesh();
  }
}

void Api::grabBatched() {
  renderMutex.lock();
}

void Api::releaseBatched() { renderMutex.unlock(); }

queue<ApiCube> *Api::getBatchedCubes(){
  return &batchedCubes;
}

queue<Line> *Api::getBatchedLines() {
  return &batchedLines;
}

Api::~Api() {
  continuePolling = false;
  context.shutdown();
  offRenderThread.join();
  delete commandServer;
}
