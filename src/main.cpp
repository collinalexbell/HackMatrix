#include <iostream>
#include <string>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <zmq/zmq.hpp>
#include <glm/glm.hpp>

#include "renderer.h"
#include "api.h"
#include "controls.h"

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

void loop (GLFWwindow* window, Renderer* renderer, Camera* camera, World* world, Api* api) {
  while(!glfwWindowShouldClose(window)) {
    renderer->render();
    api->pollFor(world);
    handleEscape(window);
    handleControls(window, camera);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

int main() {
  GLFWwindow* window = init();
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  Camera* camera = new Camera();
  World* world = new World();
  Api* api = new Api("tcp://*:5555");
  Renderer* renderer = new Renderer(camera, world);
  world->attachRenderer(renderer);
  world->addCube(glm::vec3(0, 10, 0));
  glfwSetWindowUserPointer(window, (void*)renderer);
  glfwSetCursorPosCallback(window, mouseCallback);
  if(window == NULL) {
    return -1;
  }
  world->addCube(glm::vec3(0,0,-10));
  loop(window, renderer, camera, world, api);
  glfwTerminate();
  delete renderer;
  delete world;
  delete camera;
  delete api;
  return 0;
}
