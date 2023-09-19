#include <iostream>
#include <string>

#include <glad/glad.h>
#include <glad/glad_glx.h>
#include <GLFW/glfw3.h>
#include <zmq/zmq.hpp>
#include <glm/glm.hpp>

#include "renderer.h"
#include "api.h"
#include "controls.h"
#include "camera.h"
#include "world.h"
#include "app.h"

World* world;
Api* api;
Renderer* renderer;
Controls* controls;
Camera* camera;
GLFWwindow* window;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

GLFWwindow* init() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window = glfwCreateWindow(1920, 1080, "matrix", NULL, NULL);
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
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  return window;
}

void loop () {
  while(!glfwWindowShouldClose(window)) {
    renderer->render();
    api->pollFor(world);
    controls->handleKeys(window, camera);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  std::cout << "exiting" << std::endl;
}

void mouseCallback (GLFWwindow* window, double xpos, double ypos) {
  controls->mouseCallback(window, xpos, ypos);
}

int main() {
  window = init();
  initApp();
  camera = new Camera();
  world = new World();
  api = new Api("tcp://*:5555");
  renderer = new Renderer(camera, world);
  controls = new Controls();
  world->attachRenderer(renderer);
  glfwSetWindowUserPointer(window, (void*)renderer);
  glfwSetCursorPosCallback(window, mouseCallback);
  if(window == NULL) {
    return -1;
  }
  world->addCube(glm::vec3(0,0,-10));
  loop();
  glfwTerminate();
  delete renderer;
  delete world;
  delete camera;
  delete api;
  return 0;
}
