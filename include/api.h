#ifndef __API_H__
#define __API_H__
#include <atomic>
#include <mutex>
#include <optional>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>
#include <zmq/zmq.hpp>
#include <string>
#include "entity.h"
#include "protos/api.pb.h"
#include "world.h"
#include "logger.h"

using namespace std;

// Forward-declare Wayland display to avoid pulling Wayland headers here.
struct wl_display;

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
  std::optional<int64_t> actionId;
};

struct ClearAreaAction
{
  int64_t id;
  glm::vec3 min;
  glm::vec3 max;
  std::vector<Line> lines;
  std::vector<glm::vec3> previewVoxels;
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
  World* world = nullptr;

  zmq::context_t context;
  CommandServer* commandServer;
  // Wayland display (set by wlroots path) so QUIT requests can terminate cleanly.
  wl_display* display = nullptr;

  queue<BatchedRequest> batchedRequests;

  mutex renderMutex;
  thread offRenderThread;

  std::atomic_bool continuePolling = true;
  std::atomic<int64_t> nextActionId = 1;
  std::unordered_map<int64_t, ClearAreaAction> pendingClearAreas;
  int64_t registerClearArea(const glm::vec3& min,
                            const glm::vec3& max,
                            std::optional<int64_t> requestedId);
  bool confirmClearArea(int64_t actionId);
  std::vector<Line> buildClearAreaLines(const glm::vec3& min,
                                        const glm::vec3& max) const;
  std::vector<glm::vec3> buildClearAreaVoxels(const glm::vec3& min,
                                              const glm::vec3& max) const;

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
      World* world,
      shared_ptr<WindowManager::WindowManager>);
  void setDisplay(wl_display* d) { display = d; }
  ~Api();
  void poll();
  void mutateEntities();
  int64_t allocateActionId() { return nextActionId++; }
  EngineStatus buildStatus() const;
};

#endif
