#pragma once

#include "blocks.h"
#include "camera.h"
#include <functional>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <xkbcommon/xkbcommon.h>
#include <memory>
#include "app.h"
#include "world.h"
#include "WindowManager/WindowManager.h"
#include "ControlMappings.h"

struct DeferedAction
{
  shared_ptr<bool> isDone;
  function<void()> fn;
};

struct ControlResponse
{
  bool consumed = false;
  bool blockClientDelivery = false;
  bool clearInputForces = false;
  bool clearSeatFocus = false;
   bool requestQuit = false;
};

class Controls
{
  ControlMappings controlMappings;
  shared_ptr<blocks::TexturePack> texturePack;
  WindowManager::WindowManagerPtr wm;
  float windowFlop = 0.25;
  float windowFlop_dt = 0.002;
  World* world;
  Camera* camera;
  Renderer* renderer;
  bool grabbedCursor = false;
  bool appFocused = false;
  double lastClickTime = 0;
  double lastKeyPressTime = 0;
  double lastShiftPressTime = 0;
  bool resetMouse = true;
  bool lastWaylandFocusActive = false;
  float lastX;
  float lastY;
  int clickY = 100;
  vector<DeferedAction> deferedActions;
  shared_ptr<WindowManager::Space> windowManagerSpace;
  bool keysEnabled = true;

  void handleControls(GLFWwindow* window, Camera* camera);
  void handleQuit(GLFWwindow* window);
  void handleModEscape(GLFWwindow* window);
  void handleToggleCursor(GLFWwindow* window);
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
  void handleChangePlayerSpeed(GLFWwindow* window);
  void handleWindowFlop(GLFWwindow* window);
  void handleMakeWindowBootable(GLFWwindow* window);

  void handleKeys(GLFWwindow* window, Camera* camera, World* world);
  void handleClicks(GLFWwindow* window, World* world);
  void doAfter(shared_ptr<bool> isDone, function<void()> actionFn);
  void doDeferedActions();

public:
  Controls(WindowManager::WindowManagerPtr wm,
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
    lastWaylandFocusActive = false;
  }
  void poll(GLFWwindow* window, Camera* camera, World* world);
  void mouseCallback(GLFWwindow* window, double xpos, double ypos);
  void goToApp(entt::entity);
  void moveTo(glm::vec3 pos,
              optional<glm::vec3> rotation,
              float secs,
              optional<function<void()>> = nullopt);
  ControlResponse handleKeySym(xkb_keysym_t sym,
                               bool pressed,
                               bool modifierHeld,
                               bool shiftHeld,
                               bool waylandFocusActive);
  void applyMovementInput(bool forward, bool back, bool left, bool right);
  void applyLookDelta(double dx, double dy);
  bool handlePointerButton(uint32_t button, bool pressed);
  void disableKeys();
  void enableKeys();
  void triggerScreenshot();
  void wireWindowManager(shared_ptr<WindowManager::Space>);
};
