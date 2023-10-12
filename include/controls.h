#ifndef __CONTROLS_H__
#define __CONTROLS_H__

#include "camera.h"
#include <functional>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <memory>
#include "app.h"
#include "world.h"

struct DeferedAction {
  shared_ptr<bool> isDone;
  function<void()> fn;
};

class Controls {
  bool grabbedCursor = true;
  bool appFocused = false;
  double lastToggleFocusTime = 0;
  double lastToggleAppTime = 0;
  double lastClickTime = 0;
  bool resetMouse = true;
  float lastX;
  float lastY;
  int clickY = 100;
  vector<DeferedAction> deferedActions;

  void handleControls(GLFWwindow* window, Camera* camera);
  void handleEscape(GLFWwindow* window);
  void handleToggleFocus(GLFWwindow* window);
  void handleToggleApp(GLFWwindow* window, World* world, Camera* camera);

  void handleKeys(GLFWwindow* window, Camera* camera, World* world);
  void handleClicks(GLFWwindow* window, World* world);
  void doAfter(shared_ptr<bool> isDone, function<void()> actionFn);
  void doDeferedActions();
public:
  void poll(GLFWwindow* window, Camera* camera, World* world);
  void mouseCallback (GLFWwindow* window, double xpos, double ypos);
};
#endif
