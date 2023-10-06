#ifndef __CONTROLS_H__
#define __CONTROLS_H__

#include "camera.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "app.h"
#include "world.h"

class Controls {
  bool cursorDisabled = true;
  bool appFocused = false;
  double lastToggleFocusTime = 0;
  double lastToggleAppTime = 0;
  double lastClickTime = 0;
  bool firstMouse = true;
  float lastX;
  float lastY;
  int clickY = 100;

  void handleControls(GLFWwindow* window, Camera* camera);
  void handleEscape(GLFWwindow* window);
  void handleToggleFocus(GLFWwindow* window);
  void handleToggleApp(GLFWwindow* window, World* world);

  void handleKeys(GLFWwindow* window, Camera* camera, World* world);
  void handleClicks(GLFWwindow* window, World* world);
public:
  void poll(GLFWwindow* window, Camera* camera, World* world);
  void mouseCallback (GLFWwindow* window, double xpos, double ypos);
};
#endif
