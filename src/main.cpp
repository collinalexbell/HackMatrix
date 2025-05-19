#include "engine.h"

#include <iostream>
#include <signal.h>

#include <GLFW/glfw3.h>
#include <glad/glad.h>
#include <X11/Xlib.h>

#include "screen.h"

void
framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
  glViewport(0, 0, width, height);
}

GLFWwindow* MainWindow()
{
  glfwSetErrorCallback(NULL);
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window =
    glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "matrix", NULL, NULL);
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

void
cleanup(Engine* engine)
{
  glfwTerminate();
  if(engine != NULL) {
    delete engine;
  }
}

int
main(int argc, char** argv, char** envp)
{
  try {
    GLFWwindow* window = MainWindow();
    if (window == NULL)
      return -1;

    Engine* engine = new Engine(window, envp);
    engine->loop();
    cleanup(engine);

  } catch (const std::exception& e) {
    // signal error to the trampoline
    // return -1;
    cleanup(NULL);
    throw e;
  }
}
