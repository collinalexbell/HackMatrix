#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include "controls.h"
#include "camera.h"
#include "renderer.h"
#include "app.h"

using namespace std;

void Controls::mouseCallback (GLFWwindow* window, double xpos, double ypos) {
  if(cursorDisabled) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    Renderer* renderer = (Renderer*) glfwGetWindowUserPointer(window);
    renderer->getCamera()->handleRotateForce(window, xoffset, yoffset);
  }
}

void Controls::handleKeys(GLFWwindow* window, Camera* camera) {
  handleEscape(window);
  handleControls(window, camera);
  handleToggleFocus(window);
  handleToggleApp(window);
}

void Controls::handleControls(GLFWwindow* window, Camera* camera) {

  bool up = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  camera->handleTranslateForce(up,down,left,right);
}

void Controls::handleEscape(GLFWwindow* window) {
  if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }
}

double DEBOUNCE_TIME = 0.1;
bool debounce(double& lastTime) {
  double curTime = glfwGetTime();
  double interval = curTime - lastTime;
  lastTime = curTime;
  return interval > DEBOUNCE_TIME;
}

void Controls::handleToggleApp(GLFWwindow* window) {
  if(glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
    Window x11Window = glfwGetX11Window(window);
    focus(x11Window);
  }
}

void Controls::handleToggleFocus(GLFWwindow* window) {
  if(glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
    if(debounce(lastToggleFocusTime)) {
      if(cursorDisabled) {
        cursorDisabled = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      } else {
        cursorDisabled = true;
        firstMouse = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      }
    }
  }
}
