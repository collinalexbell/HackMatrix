#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "renderer.h"
#include <zmq/zmq.hpp>
#include <glm/glm.hpp>
#include <string>

zmq::context_t context (2);
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

zmq::socket_t initZmq() {
  zmq::socket_t socket (context, zmq::socket_type::rep);
  socket.bind ("tcp://*:5555");
  return socket;
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

void handleControls(GLFWwindow* window, Camera* camera) {

  bool up = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  camera->handleTranslateForce(up,down,left,right);
}

void handleZmq(zmq::socket_t& socket, World* world) {
  zmq::message_t request;

  zmq::recv_result_t result = socket.recv(request, zmq::recv_flags::dontwait);
  if(result >= 0) {
    std::string data(static_cast<char*>(request.data()), request.size());
    std::cout  << data << std::endl;

    std::istringstream iss(data);

    std::string command;
    int x,y,z;
    iss >> command >> x >> y >> z;

    if(command == "c") {
      world->addCube(glm::vec3(x,y,z));
    }

    //  Send reply back to client
    zmq::message_t reply (5);
    memcpy (reply.data (), "recv", 5);
    socket.send (reply, zmq::send_flags::none);
  }
}

void loop (GLFWwindow* window, Renderer* renderer, Camera* camera, World* world, zmq::socket_t& socket) {
  while(!glfwWindowShouldClose(window)) {
    renderer->render();
    handleZmq(socket, world);
    handleEscape(window);
    handleControls(window, camera);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
}

void mouseCallback (GLFWwindow* window, double xpos, double ypos) {
  Renderer* renderer = (Renderer*) glfwGetWindowUserPointer(window);
  renderer->getCamera()->handleRotateForce(window, xpos, ypos);
}

int main() {
  zmq::socket_t socket = initZmq();
  GLFWwindow* window = init();
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  Camera* camera = new Camera();
  World* world = new World();
  Renderer* renderer = new Renderer(camera, world);
  world->attachRenderer(renderer);
  world->addCube(glm::vec3(0, 10, 0));
  glfwSetWindowUserPointer(window, (void*)renderer);
  glfwSetCursorPosCallback(window, mouseCallback);
  if(window == NULL) {
    return -1;
  }
  loop(window, renderer, camera, world, socket);
  glfwTerminate();
  delete renderer;
  delete world;
  delete camera;
  return 0;
}
