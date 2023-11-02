#define GLFW_EXPOSE_NATIVE_X11
#include "engine.h"
#include <GLFW/glfw3native.h>
#include <GLFW/glfw3.h>

void mouseCallback(GLFWwindow *window, double xpos, double ypos) {
  Engine *engine = (Engine *)glfwGetWindowUserPointer(window);
  engine->controls->mouseCallback(window, xpos, ypos);
}

void Engine::registerCursorCallback() {
  glfwSetWindowUserPointer(window, (void *)this);
  glfwSetCursorPosCallback(window, mouseCallback);
}

Engine::Engine(GLFWwindow* window, char** envp): window(window) {
  initialize();
  wm->createAndRegisterApps(envp);
  glfwFocusWindow(window);
  wire();
  registerCursorCallback();
}

Engine::~Engine() {
  delete controls;
  delete renderer;
  delete world;
  delete camera;
  //delete wm;
  delete api;
}

void Engine::initialize(){
  wm = new WM(glfwGetX11Window(window));
  camera = new Camera();
  world = new World(camera, true);
  api = new Api("tcp://*:3333", world);
  renderer = new Renderer(camera, world);
  controls = new Controls(wm, world, camera, renderer);
  wm->registerControls(controls);
}


void Engine::wire() {
  world->attachRenderer(renderer);
  wm->attachWorld(world);
  wm->addAppsToWorld();
  world->loadLatest();
}

void Engine::loop() {
  while (!glfwWindowShouldClose(window)) {
    renderer->render();
    api->mutateWorld();
    wm->mutateWorld();
    controls->poll(window, camera, world);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

