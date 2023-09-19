#ifndef __CONTROLS_H__
#define __CONTROLS_H__

#include "camera.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Controls {
  bool cursorDisabled = true;
  bool appFocused = false;
  double lastToggleFocusTime = 0;
  double lastToggleAppTime = 0;
  bool firstMouse = true;
  float lastX;
  float lastY;

  void handleControls(GLFWwindow* window, Camera* camera);
  void handleEscape(GLFWwindow* window);
  void handleToggleFocus(GLFWwindow* window);
  void handleToggleApp(GLFWwindow* window);
 public:
  void mouseCallback (GLFWwindow* window, double xpos, double ypos);
  void handleKeys(GLFWwindow* window, Camera* camera);
};
#endif
