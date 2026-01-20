#pragma once
#include "MultiPlayer/Client.h"
#include "MultiPlayer/Server.h"
#include "imgui/imgui.h"
#define IMGUI_IMPL_OPENGL_ES2 1
#define IMGUI_IMPL_OPENGL_LOADER_GLAD 1
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <vector>
#include "model.h"

#include "logger.h"
#include "entity.h"

using namespace std;

class Engine;
class EngineGui
{
  Engine* engine;
  GLFWwindow* window;
  shared_ptr<EntityRegistry> registry;
  shared_ptr<LoggerVector> loggerVector;
  bool useGlfwBackend = true;
  bool imguiGlInitialized = false;
  void addComponentPanel(entt::entity, bool&);
  void renderComponentPanel(entt::entity);
  void renderEntities();

public:
  EngineGui(Engine* engine,
            GLFWwindow* window,
            shared_ptr<EntityRegistry> registry,
            shared_ptr<LoggerVector>);
  void render(double&, int, vector<double>&);
  shared_ptr<LoggerVector> getLoggerVector();
  void createNewEntity();
};
