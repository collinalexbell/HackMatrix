#pragma once

#include "blocks.h"
#include "camera.h"
#include <functional>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>
#include "app.h"
#include "world.h"
#include "WindowManager/WindowManager.h"

struct DeferedAction
{
  shared_ptr<bool> isDone;
  function<void()> fn;
};

class Controls
{
  shared_ptr<blocks::TexturePack> texturePack;
  WindowManager::WindowManager* wm;
  World* world;
  Camera* camera;
  Renderer* renderer;
  bool grabbedCursor = true;
  bool appFocused = false;
  double lastClickTime = 0;
  double lastKeyPressTime = 0;
  bool resetMouse = true;
  float lastX;
  float lastY;
  int clickY = 100;
  vector<DeferedAction> deferedActions;
  shared_ptr<WindowManager::Space> windowManagerSpace;

  void handleControls(GLFWwindow* window, Camera* camera);
  void handleEscape(GLFWwindow* window);
  void handleModEscape(GLFWwindow* window);
  void handleToggleFocus(GLFWwindow* window);
  void handleToggleApp(GLFWwindow* window, World* world, Camera* camera);
  void handleSelectApp(GLFWwindow* window);
  void handleDMenu(GLFWwindow* window, World* world);
  void handleScreenshot(GLFWwindow* window);
  void handleSave(GLFWwindow* window);
  void handleSelection(GLFWwindow* window);
  void handleCodeBlock(GLFWwindow* window);
  void handleDebug(GLFWwindow* window);
  void handleToggleMeshing(GLFWwindow* window);
  void handleToggleWireframe(GLFWwindow* window);
  void handleLogBlockCounts(GLFWwindow* window);
  void handleLogBlockType(GLFWwindow* window);

  void handleKeys(GLFWwindow* window, Camera* camera, World* world);
  void handleClicks(GLFWwindow* window, World* world);
  void doAfter(shared_ptr<bool> isDone, function<void()> actionFn);
  void doDeferedActions();

public:
  Controls(WindowManager::WindowManager* wm,
           World* world,
           Camera* camera,
           Renderer* renderer,
           shared_ptr<blocks::TexturePack> texturePack)
    : wm(wm)
    , world(world)
    , camera(camera)
    , renderer(renderer)
    , texturePack(texturePack)
  {
  }
  void poll(GLFWwindow* window, Camera* camera, World* world);
  void mouseCallback(GLFWwindow* window, double xpos, double ypos);
  void goToApp(entt::entity);
  void moveTo(glm::vec3 pos,
              optional<glm::vec3> rotation,
              float secs,
              optional<function<void()>> = nullopt);
  void disable();
  void wireWindowManager(shared_ptr<WindowManager::Space>);
};
