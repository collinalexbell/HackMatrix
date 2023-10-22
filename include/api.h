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
protected:
  Api *api;
  shared_ptr<spdlog::logger> logger;
  zmq::socket_t socket;

public:
  CommandServer(Api *api, std::string bindAddress, zmq::context_t &context);
  virtual void poll(World *world) = 0;
};

class Api {

  class ProtobufCommandServer : public CommandServer {
    using CommandServer::CommandServer;
    void poll(World *world) override;
  };

  class TextCommandServer : public CommandServer {
    using CommandServer::CommandServer;
    void poll(World *world) override;
  };

  shared_ptr<spdlog::logger> logger;
  World *world;

  zmq::context_t context;
  CommandServer *legacyCommandServer;
  CommandServer *commandServer;

  queue<Cube> batchedCubes;
  queue<Line> batchedLines;

  mutex renderMutex;
  thread offRenderThread;

  std::atomic_bool continuePolling = true;

protected:
  void grabBatched();
  queue<Cube> *getBatchedCubes();
  queue<Line> *getBatchedLines();
  void releaseBatched();

public:
  Api(std::string bindAddress, World* world);
  ~Api();
  void poll();
  void mutateWorld();
  void requestWorldData(World*, string);
};

#endif
