#pragma once
#include "MultiPlayer/Client.h"
#include "MultiPlayer/Server.h"
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
  void addComponentPanel(entt::entity, bool&);
  void renderComponentPanel(entt::entity);
  void renderEntities();

public:
  EngineGui(GLFWwindow* window, shared_ptr<EntityRegistry> registry, shared_ptr<LoggerVector>);
  void render(double&, int, vector<double>&,
      std::optional<std::shared_ptr<MultiPlayer::Client>>&,
      std::optional<std::shared_ptr<MultiPlayer::Server>>&);
  shared_ptr<LoggerVector> getLoggerVector();
  void createNewEntity();
};
