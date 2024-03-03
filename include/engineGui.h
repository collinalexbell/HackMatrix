#pragma once
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <vector>
#include "model.h"

#include "logger.h"
#include "entity.h"

using namespace std;

class EngineGui {
  GLFWwindow *window;
  shared_ptr<EntityRegistry> registry;
  shared_ptr<LoggerVector> loggerVector;
public:
  EngineGui(GLFWwindow* window, shared_ptr<EntityRegistry> registry, shared_ptr<LoggerVector>);
  void render(double&, int, vector<double>&);
  shared_ptr<LoggerVector> getLoggerVector();
  void renderEntityList();
  void createNewEntity();
  void renderComponentPanel();
};
