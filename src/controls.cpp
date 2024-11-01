#include "world.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <optional>
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

void
Controls::mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
  if (grabbedCursor) {
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

void
Controls::poll(GLFWwindow* window, Camera* camera, World* world)
{
  handleKeys(window, camera, world);
  handleClicks(window, world);
  doDeferedActions();
}

void
Controls::handleKeys(GLFWwindow* window, Camera* camera, World* world)
{
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
  handleLogBlockCounts(window);
  handleLogBlockType(window);
  handleDMenu(window, world);
  handleWindowFlop(window);
}

double DEBOUNCE_TIME = 0.1;
bool
debounce(double& lastTime)
{
  double curTime = glfwGetTime();
  double interval = curTime - lastTime;
  lastTime = curTime;
  return interval > DEBOUNCE_TIME;
}

void
Controls::handleDMenu(GLFWwindow* window, World* world)
{
  // its V menu for now :(
  bool dMenuActive = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
  if (dMenuActive && debounce(lastKeyPressTime)) {
    wm->dMenu();
  }
}

void
Controls::handleLogBlockType(GLFWwindow* window)
{
  bool shouldDebug = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
  if (shouldDebug && debounce(lastKeyPressTime)) {
    world->action(LOG_BLOCK_TYPE);
  }
}

void
Controls::handleLogBlockCounts(GLFWwindow* window)
{
  bool shouldDebug = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
  if (shouldDebug && debounce(lastKeyPressTime)) {
    texturePack->logCounts();
  }
}

void
Controls::handleDebug(GLFWwindow* window)
{
  bool shouldDebug = glfwGetKey(window, GLFW_KEY_COMMA) == GLFW_PRESS;
  if (shouldDebug && debounce(lastKeyPressTime)) {
    world->mesh();
  }
}

void
Controls::handleToggleMeshing(GLFWwindow* window)
{
  bool shouldToggleMeshing = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
  if (shouldToggleMeshing && debounce(lastKeyPressTime)) {

    world->mesh(false);
  }
}

void
Controls::handleToggleWireframe(GLFWwindow* window)
{
  bool shouldToggleWireframe = glfwGetKey(window, GLFW_KEY_SLASH) == GLFW_PRESS;
  if (shouldToggleWireframe && debounce(lastKeyPressTime)) {
    renderer->toggleWireframe();
  }
}

void
Controls::handleSelection(GLFWwindow* window)
{
  bool shouldSelect = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
  if (shouldSelect && debounce(lastKeyPressTime)) {
    world->action(SELECT_CUBE);
  }
}

void
Controls::handleCodeBlock(GLFWwindow* window)
{
  bool shouldOpenCodeBlock = glfwGetKey(window, GLFW_KEY_PERIOD) == GLFW_PRESS;
  if (shouldOpenCodeBlock && debounce(lastKeyPressTime)) {
    auto newPos = camera->position + camera->front * -3.0f;
    moveTo(newPos, std::nullopt, 0.5, [this]() -> void {
      world->action(OPEN_SELECTION_CODE);
    });
  }
}

void
Controls::handleSave(GLFWwindow* window)
{
  bool shouldSave = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
  if (shouldSave && debounce(lastKeyPressTime)) {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    stringstream filenameSS;
    filenameSS << "saves/" << std::put_time(&tm, "%Y-%m-%d:%H-%M-%S.save");
    world->save(filenameSS.str());
  }
}

void
Controls::handleScreenshot(GLFWwindow* window)
{
  bool shouldCapture = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
  if (shouldCapture && debounce(lastKeyPressTime)) {
    renderer->screenshot();
  }
}

void
Controls::handleClicks(GLFWwindow* window, World* world)
{
  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
  if (state == GLFW_PRESS && debounce(lastClickTime)) {
    world->action(PLACE_CUBE);
  }

  state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  if (state == GLFW_PRESS && debounce(lastClickTime)) {
    world->action(REMOVE_CUBE);
  }
}

void
Controls::handleControls(GLFWwindow* window, Camera* camera)
{

  bool up = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
  bool down = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
  bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
  bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
  camera->handleTranslateForce(up, down, left, right);
}

void
Controls::handleEscape(GLFWwindow* window)
{
  if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
    glfwSetWindowShouldClose(window, true);
  }
}

void
Controls::handleModEscape(GLFWwindow* window)
{
  if (glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS) {
    throw "errorEscape";
  }
}

void
Controls::moveTo(glm::vec3 pos,
                 optional<glm::vec3> rotation,
                 float secs,
                 optional<function<void()>> callback)
{
  grabbedCursor = false;
  resetMouse = true;

  glm::vec3 front;
  // convert front to a rotated vector
  if (rotation.has_value()) {
    glm::quat rotationQuat = glm::quat(glm::radians(rotation.value()));
    front = rotationQuat * glm::vec3(0, 0, -1);
  }

  auto frontOrCamera = rotation.has_value() ? front : camera->front;
  shared_ptr<bool> isDone = camera->moveTo(pos, frontOrCamera, secs);
  doAfter(isDone, [this, callback]() -> void {
    if (callback.has_value()) {
      callback.value()();
    }
    grabbedCursor = true;
  });
}

void
Controls::handleWindowFlop(GLFWwindow* window) {
  if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
    windowFlop += windowFlop_dt;
  }
  if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) {
    windowFlop -= windowFlop_dt;
  }
  if(windowFlop <= 0.01) {
    windowFlop = 0.01;
  }
}


void
Controls::goToApp(entt::entity app)
{
  wm->passthroughInput();
  float deltaZ = windowManagerSpace->getViewDistanceForWindowSize(app);
  glm::vec3 rotationDegrees = windowManagerSpace->getAppRotation(app);
  glm::quat rotationQuat = glm::quat(glm::radians(rotationDegrees));

  glm::vec3 targetPosition = windowManagerSpace->getAppPosition(app);
  targetPosition = targetPosition + rotationQuat * glm::vec3(0, 0, deltaZ);
  moveTo(targetPosition, rotationDegrees, windowFlop, [app, this]() {
    wm->focusApp(app);
  });
}

void
Controls::handleToggleApp(GLFWwindow* window, World* world, Camera* camera)
{
  auto app = windowManagerSpace->getLookedAtApp();
  if (app.has_value()) {
    int rKeyPressed = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
    if (rKeyPressed && debounce(lastKeyPressTime)) {
      goToApp(app.value());
    }
  }
}

void
Controls::handleSelectApp(GLFWwindow* window)
{
  auto app = windowManagerSpace->getLookedAtApp();
  if (app) {
    int keyPressed = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
    if (keyPressed && debounce(lastKeyPressTime)) {
      windowManagerSpace->toggleAppSelect(*app);
    }
  }
}

void
Controls::doAfter(shared_ptr<bool> isDone, function<void()> actionFn)
{
  DeferedAction action;
  action.isDone = isDone;
  action.fn = actionFn;
  deferedActions.push_back(action);
}

void
Controls::doDeferedActions()
{
  vector<vector<DeferedAction>::iterator> toDelete;
  for (auto it = deferedActions.begin(); it != deferedActions.end(); it++) {
    if (*it->isDone) {
      it->fn();
      toDelete.push_back(it);
    }
  }
  for (auto it : toDelete) {
    deferedActions.erase(it);
  }
}

void
Controls::handleToggleFocus(GLFWwindow* window)
{
  if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
    if (debounce(lastKeyPressTime)) {
      if (grabbedCursor) {
        grabbedCursor = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        wm->captureInput();
      } else {
        grabbedCursor = true;
        resetMouse = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        wm->passthroughInput();
      }
    }
  }
}

void
Controls::wireWindowManager(shared_ptr<WindowManager::Space> windowManagerSpace)
{
  this->windowManagerSpace = windowManagerSpace;
}


void Controls::handleMakeWindowBootable(GLFWwindow* window) {

}
