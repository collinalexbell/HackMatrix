#ifndef __API_H__
#define __API_H__
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <zmq/zmq.hpp>
#include <string>
#include "protos/api.pb.h"
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
  virtual void poll(WorldInterface *world) = 0;
};

struct ApiCube {
  float x;
  float y;
  float z;
  int blockType;
};

struct BatchedRequest {
  static int nextId;
  BatchedRequest(ApiRequest request): request(request) {
    id = nextId++;
  }
  int64_t id;
  ApiRequest request;
};

class Api {

  class ProtobufCommandServer : public CommandServer {
    using CommandServer::CommandServer;
    void poll(WorldInterface *world) override;
  };

  shared_ptr<spdlog::logger> logger;
  WorldInterface *world;

  zmq::context_t context;
  CommandServer *commandServer;

  queue<BatchedRequest> batchedRequests;

  mutex renderMutex;
  thread offRenderThread;

  std::atomic_bool continuePolling = true;

protected:
  void grabBatched();
  queue<BatchedRequest>* getBatchedRequests();
  void releaseBatched();

public:
  Api(std::string bindAddress, WorldInterface* world);
  ~Api();
  void poll();
  void mutateWorld();
};

#endif
