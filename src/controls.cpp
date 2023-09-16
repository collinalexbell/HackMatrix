#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "controls.h"
#include "camera.h"
#include "renderer.h"

void mouseCallback (GLFWwindow* window, double xpos, double ypos) {
  Renderer* renderer = (Renderer*) glfwGetWindowUserPointer(window);
  renderer->getCamera()->handleRotateForce(window, xpos, ypos);
}

void handleControls(GLFWwindow* window, Camera* camera) {

  bool up = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  camera->handleTranslateForce(up,down,left,right);
}

void handleEscape(GLFWwindow* window) {
  if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }
}
