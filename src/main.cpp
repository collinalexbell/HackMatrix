#include "renderer.h"
#include "api.h"
#include "controls.h"
#include "camera.h"
#include "wm.h"
#include "world.h"
#include "app.h"

#include <cstddef>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <thread>


#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <glad/glad_glx.h>
#include <glm/glm.hpp>
#include <zmq/zmq.hpp>

World* world;
Api* api;
Renderer* renderer;
Controls* controls;
Camera* camera;
GLFWwindow* window;
int screen;

WM *wm;
Display *display;
Window matriXWindow;
Window overlay;

shared_ptr<spdlog::logger> logger = make_shared<spdlog::logger>("api_logger", fileSink);

#define API

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
  while (!glfwWindowShouldClose(window)) {
    renderer->render();
#ifdef API
    api->mutateWorld();
#endif
    controls->poll(window, camera, world);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

void mouseCallback (GLFWwindow* window, double xpos, double ypos) {
  controls->mouseCallback(window, xpos, ypos);
}

void createEngineObjects() {
  wm = new WM(glfwGetX11Window(window));
  camera = new Camera();
  world = new World(camera, true);
  #ifdef API
  api = new Api("tcp://*:3333", world);
  #endif
  renderer = new Renderer(camera, world);
  controls = new Controls(wm, world, camera);
  wm->registerControls(controls);
}

void wireEngineObjects() {
  world->attachRenderer(renderer);
  wm->attachWorld(world);
  wm->addAppsToWorld();
#ifdef API
  api->requestWorldData(world, "tcp://localhost:5556");
#endif
}

void registerCursorCallback() {
  glfwSetWindowUserPointer(window, (void*)renderer);
  glfwSetCursorPosCallback(window, mouseCallback);
}

void cleanup() {
  glfwTerminate();
  delete controls; delete renderer; delete world; delete camera; delete wm;
#ifdef API
  delete api;
#endif
}

void initEngine(char** envp) {
  createEngineObjects();
  wm->createAndRegisterApps(envp);
  glfwFocusWindow(window);
  wireEngineObjects();
  registerCursorCallback();
}

int main(int argc, char** argv, char** envp) {
  try {
    window = initGraphics();
    if(window == NULL) return -1;
    initEngine(envp);
    enterGameLoop();
    cleanup();
  } catch (const std::exception &e) {
    // signal the trampoline
    // to boot an xterm for rollback
    logger->error(e.what());
    logger->flush();
    return -1;
  }
}
