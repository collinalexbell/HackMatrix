#include "world.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <sstream>
#include <iomanip>
#include <ctime>

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
  handleScreenshot(window);
  handleSave(window);
  handleSelection(window);
  handleCodeBlock(window);
  handleDebug(window);
  handleToggleMeshing(window);
  handleToggleWireframe(window);
}

double DEBOUNCE_TIME = 0.1;
bool debounce(double &lastTime) {
  double curTime = glfwGetTime();
  double interval = curTime - lastTime;
  lastTime = curTime;
  return interval > DEBOUNCE_TIME;
}

void Controls::handleDebug(GLFWwindow *window) {
  bool shouldDebug = glfwGetKey(window, GLFW_KEY_COMMA) == GLFW_PRESS;
  if(shouldDebug && debounce(lastKeyPressTime)) {
    world->mesh();
  }
}

void Controls::handleToggleMeshing(GLFWwindow *window) {
  bool shouldToggleMeshing = glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS;
  if (shouldToggleMeshing && debounce(lastKeyPressTime)) {
    renderer->toggleMeshing();
  }
}

void Controls::handleToggleWireframe(GLFWwindow *window) {
  bool shouldToggleWireframe = glfwGetKey(window, GLFW_KEY_SLASH) == GLFW_PRESS;
  if (shouldToggleWireframe && debounce(lastKeyPressTime)) {
    renderer->toggleWireframe();
  }
}

void Controls::handleSelection(GLFWwindow *window){
  bool shouldSelect = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
  if (shouldSelect && debounce(lastKeyPressTime)) {
    world->action(SELECT_CUBE);
  }
}

void Controls::handleCodeBlock(GLFWwindow *window) {
  bool shouldOpenCodeBlock = glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS;
  if(shouldOpenCodeBlock && debounce(lastKeyPressTime)) {
    world->action(OPEN_SELECTION_CODE);
  }
}

void Controls::handleSave(GLFWwindow *window){
  bool shouldSave = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
  if (shouldSave && debounce(lastKeyPressTime)) {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    stringstream filenameSS;
    filenameSS << "saves/" << std::put_time(&tm, "%d-%m-%Y %H-%M-%S.save");
    world->save(filenameSS.str());
  }
}

void Controls::handleScreenshot(GLFWwindow *window) {
  bool shouldCapture = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
  if (shouldCapture && debounce(lastKeyPressTime)) {
    renderer->screenshot();
  }
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

void Controls::moveTo(glm::vec3 pos, float secs) {
  camera->moveTo(pos, camera->front, secs);
}

void Controls::goToApp(X11App* app) {
  float deltaZ = world->getViewDistanceForWindowSize(app);
  glm::vec3 targetPosition = world->getAppPosition(app);
  targetPosition.z = targetPosition.z + deltaZ;
  glm::vec3 front = glm::vec3(0, 0, -1);
  float moveSeconds = 0.25;
  resetMouse = true;
  grabbedCursor = false;
  shared_ptr<bool> isDone = camera->moveTo(targetPosition, front, moveSeconds);
  auto &grabbedCursor = this->grabbedCursor;
  doAfter(isDone, [app, &grabbedCursor, this]() {
    grabbedCursor = true;
    wm->focusApp(app);
  });
}

void Controls::handleToggleApp(GLFWwindow* window, World* world, Camera* camera) {
  X11App *app = world->getLookedAtApp();
  if(app != NULL) {
    int rKeyPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if( rKeyPressed && debounce(lastKeyPressTime)) {
      goToApp(app);
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
    if(debounce(lastKeyPressTime)) {
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

