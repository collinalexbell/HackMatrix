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

void
mouseCallback(GLFWwindow* window, double xpos, double ypos);

class Engine
{
  World* world;
  Api* api;
  Renderer* renderer;
  Controls* controls;
  Camera* camera;
  shared_ptr<WindowManager::WindowManager> wm;
  GLFWwindow* window;
  std::shared_ptr<spdlog::logger> logger;
  std::shared_ptr<EntityRegistry> registry;
  std::shared_ptr<EngineGui> engineGui;
  std::shared_ptr<MultiPlayer::Client> client;
  std::shared_ptr<MultiPlayer::Server> server;
  spdlog::sink_ptr loggerSink;
  std::shared_ptr<LoggerVector> setupLogger();

  friend void mouseCallback(GLFWwindow* window, double xpos, double ypos);
  void setupRegistry();
  void initializeMemberObjs();
  void multiplayerClientIteration(double frameStart);

public:
  Engine(GLFWwindow* window, char** envp);
  ~Engine();
  shared_ptr<EntityRegistry> getRegistry();
  void wire();
  void loop();
  void registerCursorCallback();
  void registerServer(shared_ptr<MultiPlayer::Server>);
  void registerClient(shared_ptr<MultiPlayer::Client>);
};
