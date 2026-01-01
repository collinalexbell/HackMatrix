#include "world.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <optional>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <sstream>
#include <iomanip>
#include <ctime>
#include <cstdarg>
#include <fstream>
#include <xkbcommon/xkbcommon.h>
#include <glm/gtc/quaternion.hpp>
#include <linux/input-event-codes.h>

#include "controls.h"
#include "camera.h"
#include "renderer.h"
#include "time_utils.h"

using namespace std;

namespace {
void
log_controls(const char* fmt, ...)
{
  std::ofstream out("/tmp/matrix-wlroots-output.log", std::ios::app);
  if (!out.is_open()) {
    return;
  }
  va_list args;
  va_start(args, fmt);
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  out << buf;
}
} // namespace

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
  handleQuit(window);
  if (keysEnabled) {
    handleModEscape(window);
    handleControls(window, camera);
    handleToggleCursor(window);
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
    handleChangePlayerSpeed(window);
  }
}

void
Controls::disableKeys()
{
  keysEnabled = false;
}
void
Controls::enableKeys()
{
  keysEnabled = true;
}

double DEBOUNCE_TIME = 0.1;
bool
debounce(double& lastTime)
{
  double curTime = nowSeconds();
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
    if (wm) {
      wm->menu();
    }
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
    // world->action(SELECT_CUBE);
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
  int screenshotKey = controlMappings.getKey("screenshot");
  bool shouldCapture = glfwGetKey(window, screenshotKey) == GLFW_PRESS;
  if (shouldCapture && debounce(lastKeyPressTime)) {
    renderer->screenshot();
  }
}

void
Controls::triggerScreenshot()
{
  if (renderer) {
    renderer->screenshot();
  }
}

void
Controls::handleClicks(GLFWwindow* window, World* world)
{
  int state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
  if (state == GLFW_PRESS && debounce(lastClickTime)) {
    if(grabbedCursor) {
      auto app = windowManagerSpace ? windowManagerSpace->getLookedAtApp()
                                    : std::optional<entt::entity>();
      if (app.has_value()) {
        goToApp(app.value());
      } else {
        world->action(PLACE_VOXEL);
      }
    } else {
      // move objects
      //
      // auto mouseRay = createMouseRay(mouseX, mouseY, screenWidth, screenHeight, projectionMatrix, viewMatrix)
      // do intersection
    }
  }

  state = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
  if (state == GLFW_PRESS && debounce(lastClickTime)) {
    // world->action(REMOVE_CUBE);
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
Controls::handleQuit(GLFWwindow* window)
{
  auto quitKey = controlMappings.getKey("quit");
  if (glfwGetKey(window, quitKey)) {
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
Controls::handleWindowFlop(GLFWwindow* window)
{
  if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS) {
    windowFlop += windowFlop_dt;
  }
  if (glfwGetKey(window, GLFW_KEY_9) == GLFW_PRESS) {
    windowFlop -= windowFlop_dt;
  }
  if (windowFlop <= 0.01) {
    windowFlop = 0.01;
  }
}

void
Controls::handleChangePlayerSpeed(GLFWwindow* window)
{
  auto delta = 0.05f;
  if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS &&
      debounce(lastKeyPressTime)) {
    camera->changeSpeed(delta);
  }
  if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS &&
      debounce(lastKeyPressTime)) {
    camera->changeSpeed(-delta);
  }

  int shiftPressed = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                     glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
  int zeroPressed = glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS;

  if (shiftPressed && zeroPressed && debounce(lastKeyPressTime)) {
    camera->resetSpeed();
  }
}

void
Controls::goToApp(entt::entity app)
{
  if (!wm || !windowManagerSpace) {
    return;
  }
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
  auto app = windowManagerSpace ? windowManagerSpace->getLookedAtApp()
                                : std::optional<entt::entity>();
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
  auto app = windowManagerSpace ? windowManagerSpace->getLookedAtApp()
                                : std::optional<entt::entity>();
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
Controls::handleToggleCursor(GLFWwindow* window)
{
  auto toggleCursorKey = controlMappings.getKey("toggle_cursor");
  if (glfwGetKey(window, toggleCursorKey) == GLFW_PRESS &&
      debounce(lastKeyPressTime)) {
    if (grabbedCursor) {
      grabbedCursor = false;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    } else {
      grabbedCursor = true;
      resetMouse = true;
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
  }
}

void
Controls::wireWindowManager(std::shared_ptr<WindowManager::Space> space)
{
  windowManagerSpace = space;
}

void
Controls::handleMakeWindowBootable(GLFWwindow* window)
{
}

bool
Controls::handlePointerButton(uint32_t button, bool pressed)
{
  if (!pressed) {
    return false;
  }
  if (!debounce(lastClickTime)) {
    return false;
  }
  if (button == static_cast<uint32_t>(BTN_LEFT)) {
    if (grabbedCursor) {
      auto app = windowManagerSpace ? windowManagerSpace->getLookedAtApp()
                                    : std::optional<entt::entity>();
      if (app.has_value()) {
        log_controls("controls: pointer goToApp ent=%d button=%u\n",
                     (int)entt::to_integral(app.value()),
                     button);
        goToApp(app.value());
        return true;
      }
      log_controls("controls: pointer place voxel button=%u\n", button);
      world->action(PLACE_VOXEL);
      return true;
    }
  }
  if (button == static_cast<uint32_t>(BTN_RIGHT)) {
    // Placeholder for remove voxel or alternate interaction.
    return false;
  }
  return false;
}

ControlResponse
Controls::handleKeySym(xkb_keysym_t sym,
                       bool pressed,
                       bool modifierHeld,
                       bool shiftHeld,
                       bool waylandFocusActive)
{
  ControlResponse resp;
  lastWaylandFocusActive = waylandFocusActive;
  if (!pressed) {
    return resp;
  }

  if (sym == XKB_KEY_equal || sym == XKB_KEY_plus || sym == XKB_KEY_minus ||
      sym == XKB_KEY_underscore || sym == XKB_KEY_0 || sym == XKB_KEY_9) {
    log_controls("controls: debug sym=%u pressed=%d modifier=%d shift=%d focus=%d\n",
                 sym,
                 pressed ? 1 : 0,
                 modifierHeld ? 1 : 0,
                 shiftHeld ? 1 : 0,
                 waylandFocusActive ? 1 : 0);
  }

  auto matchesConfiguredKey = [&](const std::string& fn) -> bool {
    auto keyName = controlMappings.getKeyName(fn);
    if (!keyName.has_value()) {
      return false;
    }
    xkb_keysym_t configured =
      xkb_keysym_from_name(keyName->c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
    return configured != XKB_KEY_NoSymbol && sym == configured;
  };

  auto lookedAtApp = windowManagerSpace ? windowManagerSpace->getLookedAtApp()
                                        : std::optional<entt::entity>();

  if (!waylandFocusActive && matchesConfiguredKey("quit")) {
    resp.requestQuit = true;
    resp.blockClientDelivery = true;
    resp.consumed = true;
    return resp;
  }

  // When a Wayland client is focused, let unmodified keys pass through so they reach
  // the client instead of being eaten by engine controls.
  const bool allowControls = !waylandFocusActive || modifierHeld;
  if (!allowControls) {
    return resp;
  }

  const bool toggleCursorPressed = matchesConfiguredKey("toggle_cursor") || sym == XKB_KEY_f ||
                                   sym == XKB_KEY_F;
  if (toggleCursorPressed) {
    debounce(lastKeyPressTime);
    if (grabbedCursor) {
      grabbedCursor = false;
      resetMouse = true;
      if (wm) {
        wm->captureInput();
      }
      log_controls("controls: toggle_cursor=0\n");
    } else {
      grabbedCursor = true;
      resetMouse = true;
      if (wm) {
        wm->passthroughInput();
      }
      log_controls("controls: toggle_cursor=1\n");
    }
    resp.blockClientDelivery = true;
    resp.consumed = true;
    return resp;
  }

  if (matchesConfiguredKey("screenshot") && debounce(lastKeyPressTime)) {
    triggerScreenshot();
    resp.blockClientDelivery = true;
    resp.consumed = true;
    return resp;
  }

  // Modifier-driven window manager hotkeys.
  if (modifierHeld && wm) {
    if (sym == XKB_KEY_E || sym == XKB_KEY_e) {
      wm->unfocusApp();
      resp.blockClientDelivery = true;
      resp.clearSeatFocus = true;
      resp.consumed = true;
      return resp;
    }
    if (sym >= XKB_KEY_1 && sym <= XKB_KEY_9) {
      wm->handleHotkeySym(sym, modifierHeld, shiftHeld);
      resp.blockClientDelivery = true;
      resp.consumed = true;
      return resp;
    }
    wm->handleHotkeySym(sym, modifierHeld, shiftHeld);
    resp.blockClientDelivery = true;
    resp.consumed = true;
    return resp;
  }

  switch (sym) {
    case XKB_KEY_Delete:
      throw "errorEscape";
    case XKB_KEY_r:
    case XKB_KEY_R:
      if (!waylandFocusActive && lookedAtApp.has_value()) {
        log_controls("controls: goToApp ent=%d\n", (int)entt::to_integral(lookedAtApp.value()));
        goToApp(lookedAtApp.value());
        resp.blockClientDelivery = true;
        resp.clearInputForces = true;
        resp.consumed = true;
      }
      break;
    case XKB_KEY_v:
    case XKB_KEY_V:
      if (!waylandFocusActive && wm && debounce(lastKeyPressTime)) {
        log_controls("controls: menu\n");
        wm->menu();
        resp.consumed = true;
      }
      break;
    case XKB_KEY_b:
    case XKB_KEY_B:
      if (debounce(lastKeyPressTime)) {
        texturePack->logCounts();
        log_controls("controls: logCounts\n");
        resp.consumed = true;
      }
      break;
    case XKB_KEY_t:
    case XKB_KEY_T:
      if (debounce(lastKeyPressTime)) {
        world->action(LOG_BLOCK_TYPE);
        log_controls("controls: logBlockType\n");
        resp.consumed = true;
      }
      break;
    case XKB_KEY_comma:
      if (debounce(lastKeyPressTime)) {
        world->mesh();
        log_controls("controls: mesh\n");
        resp.consumed = true;
      }
      break;
    case XKB_KEY_m:
    case XKB_KEY_M:
      if (debounce(lastKeyPressTime)) {
        world->mesh(false);
        log_controls("controls: mesh=false\n");
        resp.consumed = true;
      }
      break;
    case XKB_KEY_slash:
    case XKB_KEY_question:
      if (debounce(lastKeyPressTime)) {
        renderer->toggleWireframe();
        log_controls("controls: wireframe toggle\n");
        resp.consumed = true;
      }
      break;
    case XKB_KEY_e:
    case XKB_KEY_E:
      // Selection placeholder for parity; no-op beyond debounce.
      (void)debounce(lastKeyPressTime);
      resp.consumed = true;
      break;
    case XKB_KEY_period:
    case XKB_KEY_greater:
      if (debounce(lastKeyPressTime)) {
        auto newPos = camera->position + camera->front * -3.0f;
        moveTo(newPos, std::nullopt, 0.5, [this]() -> void {
          world->action(OPEN_SELECTION_CODE);
        });
        resp.consumed = true;
      }
      break;
    case XKB_KEY_l:
    case XKB_KEY_L:
      if (debounce(lastKeyPressTime)) {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        stringstream filenameSS;
        filenameSS << "saves/" << std::put_time(&tm, "%Y-%m-%d:%H-%M-%S.save");
        world->save(filenameSS.str());
        resp.consumed = true;
      }
      break;
    case XKB_KEY_equal:
    case XKB_KEY_plus:
      if (debounce(lastKeyPressTime)) {
        camera->changeSpeed(0.05f);
        log_controls("controls: camera speed delta=+0.05\n");
        resp.consumed = true;
      }
      break;
    case XKB_KEY_minus:
    case XKB_KEY_underscore:
      if (debounce(lastKeyPressTime)) {
        camera->changeSpeed(-0.05f);
        log_controls("controls: camera speed delta=-0.05\n");
        resp.consumed = true;
      }
      break;
    case XKB_KEY_0:
      if (shiftHeld) {
        if (debounce(lastKeyPressTime)) {
          camera->resetSpeed();
          log_controls("controls: camera speed reset\n");
          resp.consumed = true;
        }
      } else {
        windowFlop += windowFlop_dt;
        log_controls("controls: windowFlop=%.4f\n", windowFlop);
        resp.consumed = true;
      }
      break;
    case XKB_KEY_9:
      windowFlop -= windowFlop_dt;
      if (windowFlop <= 0.01) {
        windowFlop = 0.01;
      }
      log_controls("controls: windowFlop=%.4f\n", windowFlop);
      resp.consumed = true;
      break;
    default:
      break;
  }

  return resp;
}

void
Controls::applyMovementInput(bool forward, bool back, bool left, bool right)
{
  if (camera) {
    camera->handleTranslateForce(forward, back, left, right);
  }
}

void
Controls::applyLookDelta(double dx, double dy)
{
  if (renderer && renderer->getCamera()) {
    renderer->getCamera()->handleRotateForce(nullptr, dx, dy);
  }
}
