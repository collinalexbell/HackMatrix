#include "renderer.h"
#include "api.h"
#include "controls.h"
#include "camera.h"
#include "world.h"
#include "app.h"

#include <X11/X.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/xfixeswire.h>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>


#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <glad/glad.h>
#include <glad/glad_glx.h>
#include <glm/glm.hpp>
#include <zmq/zmq.hpp>

World* world;
Api* api;
Renderer* renderer;
Controls* controls;
Camera* camera;
X11App* emacs;
X11App *microsoftEdge;
X11App *obs;
X11App *terminator;
GLFWwindow* window;
Display *display;
int screen;

Window matriXWindow;
Window overlay;

#define API

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

void allow_input_passthrough(Window window){
  XserverRegion region = XFixesCreateRegion(display, NULL, 0);

  XFixesSetWindowShapeRegion(display, window, ShapeBounding, 0,0,0);
  XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, region);

  XFixesDestroyRegion(display, region);
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

  display = XOpenDisplay(NULL);
  screen = XDefaultScreen(display);
  Window root = RootWindow(display, screen);
  XCompositeRedirectSubwindows(display, RootWindow(display, screen),
                               CompositeRedirectAutomatic);

  matriXWindow = glfwGetX11Window(window);

  overlay = XCompositeGetOverlayWindow(display, root);
  XReparentWindow(display, matriXWindow, overlay, 0, 0);

  XFixesSelectCursorInput(display, overlay, XFixesDisplayCursorNotifyMask);
  allow_input_passthrough(overlay);
  allow_input_passthrough(matriXWindow);

  XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return NULL;
  }

  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  return window;
}

int enterGameLoop() {
  try {
    while(!glfwWindowShouldClose(window)) {
      renderer->render();
      #ifdef API
      api->pollFor(world);
      #endif
      controls->poll(window, camera, world);
      glfwSwapBuffers(window);
      glfwPollEvents();
    }
    return 0;
  } catch(...) {
    return -1;
  }
}

void mouseCallback (GLFWwindow* window, double xpos, double ypos) {
  controls->mouseCallback(window, xpos, ypos);
}

void createEngineObjects() {
  camera = new Camera();
  world = new World(camera, true);
  #ifdef API
  api = new Api("tcp://*:3333");
  #endif
  renderer = new Renderer(camera, world);
  controls = new Controls();
}

void wireEngineObjects() {
  world->attachRenderer(renderer);
  world->addApp(glm::vec3(2.8, 1.0, 5.0), terminator);
  world->addApp(glm::vec3(4.0,1.0,5.0), emacs);
  world->addApp(glm::vec3(4.0, 2.0, 5.0), microsoftEdge);
  world->addApp(glm::vec3(5.2, 1.0, 5.0), obs);
#ifdef API
  api->requestWorldData(world, "tcp://localhost:5556");
  #endif
}

int APP_WIDTH = 1920 * .85;
int APP_HEIGHT = 1920 * .85 * .54;
void forkOrFindApp(string cmd, string pidOf, string className, X11App*& app,char** envp) {
  char *line;
  std::size_t len = 0;
  FILE *pidPipe = popen(string("pidof " + pidOf).c_str(), "r");
  if (getline(&line, &len, pidPipe) == -1) {
    int pid = fork();
    if (pid == 0) {
      setsid();
      execle(cmd.c_str(), cmd.c_str(), NULL, envp);
      exit(0);
    }
    if(className == "obs") {
      sleep(30);
    } else {
      sleep(1);
    }
  }
  app = X11App::byClass(className, display, screen, APP_WIDTH, APP_HEIGHT);
}


void createAndRegisterApps(char** envp) {
  forkOrFindApp("/usr/bin/emacs", "emacs", "Emacs", emacs, envp);
  forkOrFindApp("/usr/bin/microsoft-edge","microsoft-edge", "Microsoft-edge", microsoftEdge, envp);
  forkOrFindApp("/usr/bin/terminator", "terminator", "Terminator", terminator, envp);
  forkOrFindApp("/usr/bin/obs", "obs", "obs", obs, envp);
  glfwFocusWindow(window);
}

void registerCursorCallback() {
  glfwSetWindowUserPointer(window, (void*)renderer);
  glfwSetCursorPosCallback(window, mouseCallback);
}

void cleanup() {
  glfwTerminate();
  //XCompositeUnredirectSubwindows(display, RootWindow(display, screen), int update);
  XCompositeReleaseOverlayWindow(display, RootWindow(display, screen));
  delete renderer; delete world;
  delete camera;
  #ifdef API
  delete api;
  #endif
  emacs->takeInputFocus();
  delete emacs;
}

void initEngine(char** envp) {
  createEngineObjects();
  createAndRegisterApps(envp);
  wireEngineObjects();
  registerCursorCallback();
}


int main(int argc, char** argv, char** envp) {
  window = initGraphics();
  if(window == NULL) {
    return -1;
  }
  initEngine(envp);
  int exit = enterGameLoop();
  cleanup();
  return exit;
}
