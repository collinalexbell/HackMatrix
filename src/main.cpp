#include "renderer.h"
#include "api.h"
#include "controls.h"
#include "camera.h"
#include "world.h"
#include "app.h"

#include <X11/X.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/xfixeswire.h>
#include <iostream>
#include <string>
#include <signal.h>

#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <GLFW/glfw3.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <glad/glad.h>
#include <glad/glad_glx.h>
#include <glm/glm.hpp>
#include <zmq/zmq.hpp>

//#define API

World* world;
Api* api;
Renderer* renderer;
Controls* controls;
Camera* camera;
X11App* emacs;
X11App *surf;
GLFWwindow* window;
Display *display;
int screen;

Window matriXWindow;
Window overlay;

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

  display = XOpenDisplay(NULL);
  screen = XDefaultScreen(display);
  XCompositeRedirectSubwindows(display, RootWindow(display, screen),
                               CompositeRedirectAutomatic);

  matriXWindow = glfwGetX11Window(window);

  overlay = XCompositeGetOverlayWindow(display, RootWindow(display, screen));
  XReparentWindow(display, matriXWindow, overlay, 0, 0);

  XFixesSelectCursorInput(display, overlay, XFixesDisplayCursorNotifyMask);

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
    #ifdef API
    api->pollFor(world);
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
  camera = new Camera();
  world = new World(camera, true);
  #ifdef API
  api = new Api("tcp://*:5555");
  #endif
  renderer = new Renderer(camera, world);
  controls = new Controls();
}

void wireEngineObjects() {
  world->attachRenderer(renderer);
  world->addApp(glm::vec3(4.0,1.0,5.5), emacs);
  world->addApp(glm::vec3(3,1.0,5.5), surf);
#ifdef API
  api->requestWorldData(world, "tcp://localhost:5556");
  #endif
}

int emacsPid = -1;
void createAndRegisterEmacs() {
  int pid = fork();
  if(pid == 0) {
    execl("/usr/bin/surf", "/usr/bin/surf", "google.com");
    exit(0);
  }
  sleep(1);
  glfwFocusWindow(window);
  emacs = new X11App("emacs@phoenix", display, screen);
  surf = new X11App("@cgDISMfxT:T", display, screen);
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
  //XCompositeUnredirectSubwindows(display, RootWindow(display, screen), int update);
  XCompositeReleaseOverlayWindow(display, RootWindow(display, screen));
  delete renderer;
  delete world;
  delete camera;
  #ifdef API
  delete api;
  #endif
  emacs->takeInputFocus();
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
