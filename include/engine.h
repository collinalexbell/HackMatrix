#include "api.h"
#include "app.h"
#include "camera.h"
#include "controls.h"
#include "renderer.h"
#include "WindowManager/WindowManager.h"
#include "world.h"
#include "entity.h"
#include "engineGui.h"

#include <GLFW/glfw3.h>
#include <memory>
#include <spdlog/common.h>


void mouseCallback(GLFWwindow *window, double xpos, double ypos);

class Engine {
  World *world;
  Api *api;
  Renderer *renderer;
  Controls *controls;
  Camera *camera;
  WindowManager::WindowManager *wm;
  GLFWwindow *window;
  std::shared_ptr<spdlog::logger> logger;
  std::shared_ptr<EntityRegistry> registry;
  std::shared_ptr<EngineGui> engineGui;
  spdlog::sink_ptr loggerSink;

  friend void mouseCallback(GLFWwindow *window, double xpos, double ypos);
  void setupRegistry();

public:
  Engine(GLFWwindow* window, char** envp);
  ~Engine();
  void initialize();
  void wire();
  void loop();
  void registerCursorCallback();
};
