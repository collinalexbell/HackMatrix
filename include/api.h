#ifndef __API_H__
#define __API_H__
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>
#include <zmq/zmq.hpp>
#include <string>
#include "entity.h"
#include "protos/api.pb.h"
#include "world.h"
#include "logger.h"

using namespace std;

namespace WindowManager {
  class WindowManager;
}
class Controls;
class Renderer;
class Api;
class CommandServer
{
protected:
  Api* api;
  shared_ptr<spdlog::logger> logger;
  zmq::socket_t socket;

public:
  CommandServer(Api* api, std::string bindAddress, zmq::context_t& context);
  virtual void poll() = 0;
};

struct ApiCube
{
  float x;
  float y;
  float z;
  int blockType;
};

struct BatchedRequest
{
  static int nextId;
  BatchedRequest(ApiRequest request)
    : request(request)
  {
    id = nextId++;
  }
  int64_t id;
  ApiRequest request;
};

class Api
{

  class ProtobufCommandServer : public CommandServer
  {
    using CommandServer::CommandServer;
    void poll() override;
  };

  Controls* controls;
  shared_ptr<WindowManager::WindowManager> wm;

  shared_ptr<spdlog::logger> logger;
  shared_ptr<EntityRegistry> registry;
  Renderer* renderer = nullptr;

  zmq::context_t context;
  CommandServer* commandServer;

  queue<BatchedRequest> batchedRequests;

  mutex renderMutex;
  thread offRenderThread;

  std::atomic_bool continuePolling = true;

protected:
  void grabBatched();
  queue<BatchedRequest>* getBatchedRequests();
  void releaseBatched();
  void processBatchedRequest(BatchedRequest);

public:
  Api(std::string bindAddress,
      shared_ptr<EntityRegistry>,
      Controls* controls,
      Renderer* renderer,
      shared_ptr<WindowManager::WindowManager>);
  ~Api();
  void poll();
  void mutateEntities();
};

#endif
