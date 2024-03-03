#include "api.h"
#include "app.h"
#include "camera.h"
#include "controls.h"
#include "renderer.h"
#include "wm.h"
#include "world.h"
#include "entity.h"

#include <GLFW/glfw3.h>
#include <memory>
#include <spdlog/common.h>

void mouseCallback(GLFWwindow *window, double xpos, double ypos);

class Engine {
  shared_ptr<LoggerVector> loggerVector;
  World *world;
  Api *api;
  Renderer *renderer;
  Controls *controls;
  Camera *camera;
  WM *wm;
  GLFWwindow *window;
  std::shared_ptr<spdlog::logger> logger;
  std::shared_ptr<EntityRegistry> registry;
  spdlog::sink_ptr loggerSink;

  friend void mouseCallback(GLFWwindow *window, double xpos, double ypos);
  void initImGui();
  void renderImGui(double &fps, int frameIndex, const vector<double> &frameTimes);

public:
  Engine(GLFWwindow* window, char** envp);
  ~Engine();
  void initialize();
  void wire();
  void loop();
  void registerCursorCallback();
};
