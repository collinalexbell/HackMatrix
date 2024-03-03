#pragma once
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <vector>

#include "logger.h"

using namespace std;

class EngineGui {
  GLFWwindow *window;
  shared_ptr<LoggerVector> loggerVector;
public:
  EngineGui(GLFWwindow* window);
  void render(double&, int, vector<double>&);
  shared_ptr<LoggerVector> getLoggerVector();
};
