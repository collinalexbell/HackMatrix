#include "renderer.h"
#include "api.h"
#include "controls.h"
#include "camera.h"
#include "world.h"
#include "app.h"

#include <iostream>
#include <string>
#include <signal.h>

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
X11App* emacs;
GLFWwindow* window;

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

GLFWwindow* initGraphics() {
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

void enterGameLoop() {
  while(!glfwWindowShouldClose(window)) {
    renderer->render();
    //api->pollFor(world);
    controls->poll(window, camera, world);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

void mouseCallback (GLFWwindow* window, double xpos, double ypos) {
  controls->mouseCallback(window, xpos, ypos);
}

void createEngineObjects() {
  camera = new Camera();
  world = new World(camera);
  //api = new Api("tcp://*:5555");
  renderer = new Renderer(camera, world);
  controls = new Controls();
}

void wireEngineObjects() {
  world->attachRenderer(renderer);
  world->addAppCube(glm::vec3(4,1,5.5));
  //api->requestWorldData(world, "tcp://localhost:5556");
}

int emacsPid = -1;
void createAndRegisterEmacs() {
  int pid = fork();
  if(pid == 0) {
    //execl("", "");
    exit(0);
  }
  emacs = new X11App("emacs@phoenix");
  renderer->registerApp(emacs);
  controls->registerApp(emacs);
}

void registerCursorCallback() {
  glfwSetWindowUserPointer(window, (void*)renderer);
  glfwSetCursorPosCallback(window, mouseCallback);
}

void cleanup() {
  glfwTerminate();
  if(emacsPid != -1) {
    kill(emacsPid, SIGKILL);
  }
  delete renderer;
  delete world;
  delete camera;
  delete api;
  delete emacs;
}

void initEngine() {
  createEngineObjects();
  createAndRegisterEmacs();
  wireEngineObjects();
  registerCursorCallback();
}


int main() {
  window = initGraphics();
  if(window == NULL) {
    return -1;
  }
  initEngine();
  enterGameLoop();
  cleanup();
  return 0;
}
