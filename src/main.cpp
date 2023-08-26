#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "renderer.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

GLFWwindow* init() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window = glfwCreateWindow(800, 600, "matrix", NULL, NULL);
  if (window == NULL) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return NULL;
  }
  glfwMakeContextCurrent(window);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return NULL;
  }
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  return window;
}

void handleEscape(GLFWwindow* window) {
  if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, true);
  }
}

void loop (GLFWwindow* window, Renderer* renderer) {
  while(!glfwWindowShouldClose(window)) {
    renderer->render();
    handleEscape(window);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

int main() {
  GLFWwindow* window = init();
  Renderer* renderer = new Renderer();
  if(window == NULL) {
    return -1;
  }
  loop(window, renderer);
  delete renderer;
  glfwTerminate();
  return 0;
}
