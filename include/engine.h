#include "api.h"
#include "app.h"
#include "camera.h"
#include "controls.h"
#include "renderer.h"
#include "WindowManager/WindowManager.h"
#include "world.h"
#include "entity.h"
#include "engineGui.h"
#include "MultiPlayer/Client.h"
#include "MultiPlayer/Server.h"

#include <GLFW/glfw3.h>
#include <memory>
#include <spdlog/common.h>
#include <vector>

void
mouseCallback(GLFWwindow* window, double xpos, double ypos);

struct EngineOptions {
  bool enableGui = true;
  bool enableControls = true;
};

class Engine
{
  World* world;
  Api* api;
  Renderer* renderer;
  Controls* controls;
  Camera* camera;
  shared_ptr<WindowManager::WindowManager> wm;
  GLFWwindow* window;
  EngineOptions options;
  std::vector<double> frameTimes;
  int frameIndex = 0;
  double fps = 0.0;
  std::shared_ptr<spdlog::logger> logger;
  std::shared_ptr<EntityRegistry> registry;
  std::shared_ptr<EngineGui> engineGui;
  std::shared_ptr<MultiPlayer::Client> client;
  std::shared_ptr<MultiPlayer::Server> server;
  spdlog::sink_ptr loggerSink;
  std::shared_ptr<LoggerVector> setupLogger();
  void disableKeysIfImguiActive();

  friend void mouseCallback(GLFWwindow* window, double xpos, double ypos);
  void setupRegistry();
  void initializeMemberObjs();
  void multiplayerClientIteration(double frameStart);

public:
  Engine(GLFWwindow* window, char** envp, EngineOptions options = {});
  ~Engine();
  shared_ptr<EntityRegistry> getRegistry();
  void wire();
  void loop();
  void frame(double frameStart);
  void registerCursorCallback();
  void registerServer(shared_ptr<MultiPlayer::Server>);
  void registerClient(shared_ptr<MultiPlayer::Client>);
};
