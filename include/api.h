#ifndef __API_H__
#define __API_H__
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <zmq/zmq.hpp>
#include <string>
#include "world.h"
#include "logger.h"

using namespace std;

class Api;
class CommandServer {
  zmq::socket_t socket;
  Api *api;
  shared_ptr<spdlog::logger> logger;

public:
  void legacyPollForWorldCommands(World* world); void pollForWorldCommands(World *world);
  CommandServer(Api* api, std::string bindAddress, zmq::context_t& context);
  void poll(World* world);
};

class Api {
  queue<Cube> batchedCubes;
  World* world;
  mutex renderMutex;
  thread offRenderThread;
  std::atomic_bool continuePolling = true;
  zmq::context_t context;
  CommandServer* legacyCommandServer;
  CommandServer *commandServer;
  shared_ptr<spdlog::logger> logger;

public:
  Api(std::string bindAddress, World* world);
  ~Api();
  void pollFor();
  void requestWorldData(World*, string);
  void mutateWorld();
  queue<Cube> *grabBatchedCubes();
  void releaseBatchedCubes();
};

#endif
