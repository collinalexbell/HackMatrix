#include "renderer.h"
#include "api.h"
#include "controls.h"
#include "camera.h"
#include "world.h"
#include "app.h"

#include <iostream>
#include <string>

#include <glad/glad.h>
#include <glad/glad_glx.h>
#include <GLFW/glfw3.h>
#include <zmq/zmq.hpp>
#include <glm/glm.hpp>

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

void initWorld(World* world) {
  for(float i=-0.5; i<=0.4; i = i+0.1) {
    world->addCube(glm::vec3(0.6,i,-10.5),0);
    world->addCube(glm::vec3(-0.6,i,-10.5),0);
  }
  for(float i = -0.6; i < 0.6; i = i+0.1) {
    world->addCube(glm::vec3(i,0.4,-10.5), 0);
  }

  for(float z=-25; z<0; z = z+0.1) {
    for(float i = -0.6; i < 0.6; i = i+0.1) {
      world->addCube(glm::vec3(i,-0.5,z), 1);
    }
    for(float i=-0.5; i<=0.4; i = i+0.1) {
      world->addCube(glm::vec3(0.6,i,z),0);
      world->addCube(glm::vec3(-0.6,i,z),0);
    }
    for(float i = -0.6; i < 0.6; i = i+0.1) {
      world->addCube(glm::vec3(i,0.4,z), 0);
    }
  }
}

int main() {
  window = init();
  int maxTextureImageUnits;
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxTextureImageUnits);
  std::cout << "max textures: " << maxTextureImageUnits << endl;
  X11App emacs("emacs@phoenix");
  camera = new Camera();
  world = new World();
  api = new Api("tcp://*:5555");
  renderer = new Renderer(camera, world);
  renderer->registerApp(&emacs);
  controls = new Controls();
  controls->registerApp(&emacs);
  world->attachRenderer(renderer);
  //initWorld(world);
  world->addAppCube(glm::vec3(0,0,0));
  api->initWorld(world, "tcp://localhost:5556");
  glfwSetWindowUserPointer(window, (void*)renderer);
  glfwSetCursorPosCallback(window, mouseCallback);
  if(window == NULL) {
    return -1;
  }
  loop();
  glfwTerminate();
  delete renderer;
  delete world;
  delete camera;
  delete api;
  return 0;
}
