#include "world.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include "controls.h"
#include "camera.h"
#include "renderer.h"
#include "app.h"

using namespace std;

void Controls::mouseCallback (GLFWwindow* window, double xpos, double ypos) {
  if(grabbedCursor) {
    if (resetMouse) {
        lastX = xpos;
        lastY = ypos;
        resetMouse = false;
    }
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    Renderer* renderer = (Renderer*) glfwGetWindowUserPointer(window);
    renderer->getCamera()->handleRotateForce(window, xoffset, yoffset);
  }
}

void Controls::poll(GLFWwindow* window, Camera* camera, World* world) {
  handleKeys(window, camera, world);
  handleClicks(window, world);
  doDeferedActions();
}

void Controls::handleKeys(GLFWwindow *window, Camera *camera, World* world) {
  handleEscape(window);
  handleModEscape(window);
  handleControls(window, camera);
  handleToggleFocus(window);
  handleToggleApp(window, world, camera);
}

double DEBOUNCE_TIME = 0.1;
bool debounce(double &lastTime) {
  double curTime = glfwGetTime();
  double interval = curTime - lastTime;
  lastTime = curTime;
  return interval > DEBOUNCE_TIME;
}

void Controls::handleClicks(GLFWwindow* window, World* world) {
  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
  if (state == GLFW_PRESS && debounce(lastClickTime)) {
      world->action(PLACE_CUBE);
  }

  state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  if (state == GLFW_PRESS && debounce(lastClickTime)) {
    world->action(REMOVE_CUBE);
  }
}

void Controls::handleControls(GLFWwindow* window, Camera* camera) {

  bool up = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  camera->handleTranslateForce(up,down,left,right);
}

void Controls::handleEscape(GLFWwindow* window) {
  if(glfwGetKey(window, GLFW_KEY_ESCAPE)) {
    glfwSetWindowShouldClose(window, true);
  }
}

void Controls::handleModEscape(GLFWwindow* window) {
  if (glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS) {
    throw "errorEscape";
  }
}

void Controls::handleToggleApp(GLFWwindow* window, World* world, Camera* camera) {
  X11App* app = world->getLookedAtApp();
  int eKeyPressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
  int rKeyPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
  if((eKeyPressed || rKeyPressed) && debounce(lastToggleAppTime)) {
    // seperate if statements to detect which key, because must call
    // debounce only once
    if (app != NULL && eKeyPressed) {
      Window x11Window = glfwGetX11Window(window);
      app->focus(x11Window);
    } else if (app != NULL && rKeyPressed) {
      Window x11Window = glfwGetX11Window(window);
      float deltaZ = world->getViewDistanceForWindowSize(app);
      glm::vec3 targetPosition = world->getAppPosition(app);
      targetPosition.z = targetPosition.z + deltaZ;
      glm::vec3 front = glm::vec3(0, 0, -1);
      float moveSeconds = 0.25;
      resetMouse = true;
      grabbedCursor = false;
      shared_ptr<bool> isDone =
          camera->moveTo(targetPosition, front, moveSeconds);
      auto &grabbedCursor = this->grabbedCursor;
      doAfter(isDone, [app, x11Window, &grabbedCursor]() {
        grabbedCursor = true;
        app->focus(x11Window);
      });
    }
  }
}

void Controls::doAfter(shared_ptr<bool> isDone, function<void()> actionFn) {
  DeferedAction action;
  action.isDone = isDone;
  action.fn = actionFn;
  deferedActions.push_back(action);
}

void Controls::doDeferedActions() {
  vector<vector<DeferedAction>::iterator> toDelete;
  for(auto it=deferedActions.begin(); it!=deferedActions.end(); it++) {
    if(*it->isDone) {
      it->fn();
      toDelete.push_back(it);
    }
  }
  for(auto it: toDelete) {
    deferedActions.erase(it);
  }
}

void Controls::handleToggleFocus(GLFWwindow* window) {
  if(glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
    if(debounce(lastToggleFocusTime)) {
      if(grabbedCursor) {
        grabbedCursor = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      } else {
        grabbedCursor = true;
        resetMouse = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      }
    }
  }
}

