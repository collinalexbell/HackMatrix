#include "world.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <optional>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cctype>
#include <cstdarg>
#include <fstream>
#include <mutex>
#include <xkbcommon/xkbcommon.h>
#include <glm/gtc/quaternion.hpp>
#include <linux/input-event-codes.h>

#include "controls.h"
#include "camera.h"
#include "renderer.h"
#include "time_utils.h"

using namespace std;

namespace {
std::optional<int>
glfw_key_from_name(const std::string& keyName)
{
  if (keyName.size() == 1) {
    const unsigned char ch = static_cast<unsigned char>(keyName[0]);
    if (std::isalpha(ch)) {
      return GLFW_KEY_A + (std::toupper(ch) - 'A');
    }
    if (std::isdigit(ch)) {
      return GLFW_KEY_0 + (ch - '0');
    }
    switch (ch) {
      case ',':
        return GLFW_KEY_COMMA;
      case '.':
        return GLFW_KEY_PERIOD;
      case '/':
        return GLFW_KEY_SLASH;
      case ';':
        return GLFW_KEY_SEMICOLON;
      case '\'':
        return GLFW_KEY_APOSTROPHE;
      case '[':
        return GLFW_KEY_LEFT_BRACKET;
      case ']':
        return GLFW_KEY_RIGHT_BRACKET;
      case '\\':
        return GLFW_KEY_BACKSLASH;
      case '=':
        return GLFW_KEY_EQUAL;
      case '-':
        return GLFW_KEY_MINUS;
      case '`':
        return GLFW_KEY_GRAVE_ACCENT;
      default:
        return std::nullopt;
    }
  }

  if (keyName == "escape") {
    return GLFW_KEY_ESCAPE;
  }
  if (keyName == "delete") {
    return GLFW_KEY_DELETE;
  }
  if (keyName == "space") {
    return GLFW_KEY_SPACE;
  }
  if (keyName == "tab") {
    return GLFW_KEY_TAB;
  }
  if (keyName == "enter" || keyName == "return") {
    return GLFW_KEY_ENTER;
  }
  if (keyName == "backspace") {
    return GLFW_KEY_BACKSPACE;
  }

  return std::nullopt;
}

int
configured_or_fallback_glfw_key(ControlMappings& controlMappings,
                                const std::string& fn,
                                int fallback)
{
  if (auto keyName = controlMappings.getKeyName(fn)) {
    if (auto glfwKey = glfw_key_from_name(*keyName)) {
      return *glfwKey;
    }
  }
  return fallback;
}

bool
is_configured_or_fallback_pressed(ControlMappings& controlMappings,
                                  const std::unordered_set<xkb_keysym_t>& pressed,
                                  const std::string& fn,
                                  xkb_keysym_t fallbackLower,
                                  xkb_keysym_t fallbackUpper)
{
  int mapped = controlMappings.getKey(fn);
  if (mapped != -1) {
    return pressed.count(static_cast<xkb_keysym_t>(mapped)) > 0;
  }
  return pressed.count(fallbackLower) > 0 || pressed.count(fallbackUpper) > 0;
}

bool
matches_configured_or_fallback_key(ControlMappings& controlMappings,
                                   xkb_keysym_t sym,
                                   const std::string& fn,
                                   xkb_keysym_t fallbackLower,
                                   xkb_keysym_t fallbackUpper)
{
  int mapped = controlMappings.getKey(fn);
  if (mapped != -1) {
    return sym == static_cast<xkb_keysym_t>(mapped);
  }
  return sym == fallbackLower || sym == fallbackUpper;
}

bool
is_shifted_digit_sym(xkb_keysym_t sym)
{
  switch (sym) {
    case XKB_KEY_exclam:
    case XKB_KEY_at:
    case XKB_KEY_numbersign:
    case XKB_KEY_dollar:
    case XKB_KEY_percent:
    case XKB_KEY_asciicircum:
    case XKB_KEY_ampersand:
    case XKB_KEY_asterisk:
    case XKB_KEY_parenleft:
      return true;
    default:
      return false;
  }
}

xkb_keysym_t
normalize_hotkey_sym(xkb_keysym_t sym)
{
  switch (sym) {
    case XKB_KEY_exclam:
      return XKB_KEY_1;
    case XKB_KEY_at:
      return XKB_KEY_2;
    case XKB_KEY_numbersign:
      return XKB_KEY_3;
    case XKB_KEY_dollar:
      return XKB_KEY_4;
    case XKB_KEY_percent:
      return XKB_KEY_5;
    case XKB_KEY_asciicircum:
      return XKB_KEY_6;
    case XKB_KEY_ampersand:
      return XKB_KEY_7;
    case XKB_KEY_asterisk:
      return XKB_KEY_8;
    case XKB_KEY_parenleft:
      return XKB_KEY_9;
    default:
      return sym;
  }
}

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
Controls::poll() {
  runQueuedActions();
  pollPressedKeys();
  handleKeys();
  doDeferedActions();
}

void
Controls::poll(GLFWwindow* window, Camera* camera, World* world)
{
  runQueuedActions();
  handleKeys(window, camera, world);
  handleClicks(window, world);
  doDeferedActions();
}

// wayland version
void Controls::handleKeys() {
  auto appFocused = wm->hasCurrentOrPendingFocus();
  if (!appFocused) {
    handleMovement();
    handleToggleApp();
  }

  if (!keysEnabled) {
    return;
  }

  const bool allowControls = !appFocused || waylandModifierHeld;
  if (!allowControls) {
    return;
  }

  handleModEscape();
  handleDMenu();
  handleToggleCursor();
  handleScreenshot();
  if (handleWaylandHotkeys()) {
    return;
  }

  handleSave();
  handleSelection();
  handleCodeBlock();
  handleDebug();
  handleToggleMeshing();
  handleToggleWireframe();
  handleLogBlockCounts();
  handleLogBlockType();
  handleWindowFlop();
  handleChangePlayerSpeed();
}

// glfw version (to be deleted, maybe)
void
Controls::handleKeys(GLFWwindow* window, Camera* camera, World* world)
{
  //handleQuit(window);
  if (keysEnabled) {
    handleModEscape(window);
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
Controls::handleDMenu()
{
  bool dMenuActive = isPressedEither(XKB_KEY_v, XKB_KEY_V);
  if (dMenuActive && debounce(lastKeyPressTime)) {
    if (wm) {
      log_controls("controls: menu\n");
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
Controls::handleLogBlockType()
{
  bool shouldDebug = isPressedEither(XKB_KEY_t, XKB_KEY_T);
  if (shouldDebug && debounce(lastKeyPressTime)) {
    world->action(LOG_BLOCK_TYPE);
    log_controls("controls: logBlockType\n");
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
Controls::handleLogBlockCounts()
{
  bool shouldDebug = isPressedEither(XKB_KEY_b, XKB_KEY_B);
  if (shouldDebug && debounce(lastKeyPressTime)) {
    texturePack->logCounts();
    log_controls("controls: logCounts\n");
  }
}

void
Controls::handleDebug(GLFWwindow* window)
{
  int debugKey =
    configured_or_fallback_glfw_key(controlMappings, "debug_mesh", GLFW_KEY_COMMA);
  bool shouldDebug = glfwGetKey(window, debugKey) == GLFW_PRESS;
  if (shouldDebug && debounce(lastKeyPressTime)) {
    world->mesh();
  }
}

void
Controls::handleDebug()
{
  bool shouldDebug = is_configured_or_fallback_pressed(
    controlMappings, pressed, "debug_mesh", XKB_KEY_comma, XKB_KEY_comma);
  if (shouldDebug && debounce(lastKeyPressTime)) {
    world->mesh();
    log_controls("controls: mesh\n");
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
Controls::handleToggleMeshing()
{
  bool shouldToggleMeshing = isPressedEither(XKB_KEY_m, XKB_KEY_M);
  if (shouldToggleMeshing && debounce(lastKeyPressTime)) {
    world->mesh(false);
    log_controls("controls: mesh=false\n");
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
Controls::handleToggleWireframe()
{
  bool shouldToggleWireframe =
    isPressed(XKB_KEY_slash) || isPressed(XKB_KEY_question);
  if (shouldToggleWireframe && debounce(lastKeyPressTime)) {
    renderer->toggleWireframe();
    log_controls("controls: wireframe toggle\n");
  }
}

void
Controls::handleSelection(GLFWwindow* window)
{
  int selectKey =
    configured_or_fallback_glfw_key(controlMappings, "select", GLFW_KEY_E);
  bool shouldSelect = glfwGetKey(window, selectKey) == GLFW_PRESS;
  if (shouldSelect && debounce(lastKeyPressTime)) {
    // world->action(SELECT_CUBE);
  }
}

void
Controls::handleSelection()
{
  bool shouldSelect = is_configured_or_fallback_pressed(
    controlMappings, pressed, "select", XKB_KEY_e, XKB_KEY_E);
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
Controls::handleCodeBlock()
{
  bool shouldOpenCodeBlock = isPressed(XKB_KEY_period) || isPressed(XKB_KEY_greater);
  if (shouldOpenCodeBlock && debounce(lastKeyPressTime)) {
    auto newPos = camera->position + camera->front * -3.0f;
    log_controls("controls: code block move distance=3.0\n");
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
Controls::handleSave()
{
  bool shouldSave = isPressedEither(XKB_KEY_l, XKB_KEY_L);
  if (shouldSave && debounce(lastKeyPressTime)) {
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    stringstream filenameSS;
    filenameSS << "saves/" << std::put_time(&tm, "%Y-%m-%d:%H-%M-%S.save");
    world->save(filenameSS.str());
    log_controls("controls: save %s\n", filenameSS.str().c_str());
  }
}

void
Controls::handleScreenshot(GLFWwindow* window)
{
  int screenshotKey =
    configured_or_fallback_glfw_key(controlMappings, "screenshot", GLFW_KEY_P);
  bool shouldCapture = glfwGetKey(window, screenshotKey) == GLFW_PRESS;
  if (shouldCapture && debounce(lastKeyPressTime)) {
    triggerScreenshot();
  }
}

void
Controls::handleScreenshot()
{
  int screenshotKey = controlMappings.getKey("screenshot");
  bool shouldCapture = screenshotKey != -1 && isPressed(static_cast<xkb_keysym_t>(screenshotKey));
  if (shouldCapture && debounce(lastKeyPressTime)) {
    log_controls("controls: screenshot\n");
    triggerScreenshot();
  }
}

void
Controls::triggerScreenshot()
{
  wm->requestScreenshot();
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
Controls::handleFocus()
{
  if (pressed.count(XKB_KEY_r) > 0 && debounce(lastKeyPressTime)) {
    if (auto looked = windowManagerSpace->getLookedAtApp()) {
      goToApp(*looked);
    }
  }
}

void
Controls::handleMovement()
{
  bool zPlus = pressed.count(XKB_KEY_e) > 0;
  bool zNegative = pressed.count(XKB_KEY_e) > 0;
  bool up = is_configured_or_fallback_pressed(
    controlMappings, pressed, "move_forward", XKB_KEY_w, XKB_KEY_W);
  bool down = is_configured_or_fallback_pressed(
    controlMappings, pressed, "move_back", XKB_KEY_s, XKB_KEY_S);
  bool left = is_configured_or_fallback_pressed(
    controlMappings, pressed, "move_left", XKB_KEY_a, XKB_KEY_A);
  bool right = is_configured_or_fallback_pressed(
    controlMappings, pressed, "move_right", XKB_KEY_d, XKB_KEY_D);
  // I need to change this to get zPlus and zNegative working
  camera->handleTranslateForce(up, down, left, right, zPlus, zNegative);
}

void
Controls::handleModEscape(GLFWwindow* window)
{
  if (glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS) {
    throw "errorEscape";
  }
}

void
Controls::handleModEscape()
{
  if (isPressed(XKB_KEY_Delete)) {
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
Controls::handleWindowFlop()
{
  if (isPressed(XKB_KEY_0) &&
      !(waylandShiftHeld || (nowSeconds() - lastShiftPressTime) < 0.25)) {
    windowFlop += windowFlop_dt;
  }
  if (isPressed(XKB_KEY_9)) {
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
Controls::handleChangePlayerSpeed()
{
  auto delta = 0.05f;
  if ((isPressed(XKB_KEY_equal) || isPressed(XKB_KEY_plus)) &&
      debounce(lastKeyPressTime)) {
    camera->changeSpeed(delta);
    log_controls("controls: camera speed delta=+0.05\n");
  }
  if ((isPressed(XKB_KEY_minus) || isPressed(XKB_KEY_underscore)) &&
      debounce(lastKeyPressTime)) {
    camera->changeSpeed(-delta);
    log_controls("controls: camera speed delta=-0.05\n");
  }

  bool zeroPressed = isPressed(XKB_KEY_0);
  if (zeroPressed &&
      (waylandShiftHeld || (nowSeconds() - lastShiftPressTime) < 0.25) &&
      debounce(lastKeyPressTime)) {
    camera->resetSpeed();
    log_controls("controls: camera speed reset\n");
  }
}

void
Controls::goToApp(entt::entity app)
{
  if (!wm || !windowManagerSpace || !registry || !registry->valid(app) ||
      !registry->all_of<Positionable>(app)) {
    return;
  }
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
  int toggleCursorKey =
    configured_or_fallback_glfw_key(controlMappings, "toggle_cursor", GLFW_KEY_F);
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
Controls::handleToggleCursor()
{
  auto toggleCursorKey = controlMappings.getKey("toggle_cursor");
  bool toggleCursorPressed =
    (toggleCursorKey != -1 &&
     isPressed(static_cast<xkb_keysym_t>(toggleCursorKey))) ||
    isPressedEither(XKB_KEY_f, XKB_KEY_F);
  if (toggleCursorPressed && debounce(lastKeyPressTime)) {
    if (grabbedCursor) {
      grabbedCursor = false;
      resetMouse = true;
      if (wm) {
        wm->setCursorVisible(true);
      }
      log_controls("controls: toggle_cursor=0\n");
    } else {
      grabbedCursor = true;
      resetMouse = true;
      if (wm) {
        wm->setCursorVisible(false);
      }
      log_controls("controls: toggle_cursor=1\n");
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
        goToApp(app.value());
        return true;
      }
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

void Controls::pollPressedKeys() {
  std::lock_guard<std::mutex> lock(keysymMutex);
  KeysymEvent keysymEvent;
  while(keysymQueue.size() > 0) {
    keysymEvent = keysymQueue.front();
    waylandModifierHeld = keysymEvent.modifierHeld;
    waylandShiftHeld = keysymEvent.shiftHeld;
    if (waylandShiftHeld) {
      lastShiftPressTime = nowSeconds();
    }
    if(keysymEvent.pressed) {
      pressed.insert(keysymEvent.sym); 
    } else {
      pressed.erase(keysymEvent.sym);
    }
    keysymQueue.pop();
  }
}


// called by the wayland server when input is detected
ControlResponse
Controls::handleKeySym(xkb_keysym_t sym,
                       bool is_pressed,
                       bool modifierHeld,
                       bool shiftHeld)
{
  {
    std::lock_guard<std::mutex> lock(keysymMutex);
    keysymQueue.push(KeysymEvent{sym, is_pressed, modifierHeld, shiftHeld});
  }

  ControlResponse resp;
  if (!is_pressed) {
    return resp;
  }

  if (sym == XKB_KEY_Shift_L || sym == XKB_KEY_Shift_R || shiftHeld) {
    lastShiftPressTime = nowSeconds();
  }

  bool waylandFocusActive = wm->hasCurrentOrPendingFocus();
  if (waylandFocusActive && !modifierHeld) {
    return resp;
  }

  if (matchesConfiguredKey(sym, "quit")) {
    resp.requestQuit = true;
    resp.blockClientDelivery = true;
    resp.consumed = true;
    return resp;
  }

  const bool screenshotKey = matchesConfiguredKey(sym, "screenshot");
  const bool toggleCursorKey =
    matches_configured_or_fallback_key(
      controlMappings, sym, "toggle_cursor", XKB_KEY_f, XKB_KEY_F);
  const xkb_keysym_t hotkeySym = normalize_hotkey_sym(sym);
  if (sym == XKB_KEY_v || sym == XKB_KEY_V ||
      screenshotKey ||
      toggleCursorKey) {
    if (sym == XKB_KEY_v || sym == XKB_KEY_V) {
      resp.clearInputForces = true;
    }
    resp.blockClientDelivery = true;
    resp.consumed = true;
    return resp;
  }

  if (modifierHeld && wm) {
    if (matches_configured_or_fallback_key(
          controlMappings, sym, "unfocus_app", XKB_KEY_e, XKB_KEY_E)) {
      resp.clearSeatFocus = true;
      resp.blockClientDelivery = true;
      resp.consumed = true;
      return resp;
    }
    if (hotkeySym >= XKB_KEY_1 && hotkeySym <= XKB_KEY_9) {
      resp.clearInputForces = true;
      resp.clearSeatFocus = true;
      resp.blockClientDelivery = true;
      resp.consumed = true;
      return resp;
    }
    if (sym == XKB_KEY_p || sym == XKB_KEY_P ||
        sym == XKB_KEY_q || sym == XKB_KEY_Q ||
        sym == XKB_KEY_0 || is_shifted_digit_sym(sym)) {
      resp.blockClientDelivery = true;
      resp.consumed = true;
      return resp;
    }
  }

  const bool regularControl =
    sym == XKB_KEY_b || sym == XKB_KEY_B ||
    sym == XKB_KEY_t || sym == XKB_KEY_T ||
    matches_configured_or_fallback_key(
      controlMappings, sym, "debug_mesh", XKB_KEY_comma, XKB_KEY_comma) ||
    sym == XKB_KEY_m || sym == XKB_KEY_M ||
    sym == XKB_KEY_slash || sym == XKB_KEY_question ||
    matches_configured_or_fallback_key(
      controlMappings, sym, "select", XKB_KEY_e, XKB_KEY_E) ||
    sym == XKB_KEY_period || sym == XKB_KEY_greater ||
    sym == XKB_KEY_l || sym == XKB_KEY_L ||
    sym == XKB_KEY_equal || sym == XKB_KEY_plus ||
    sym == XKB_KEY_minus || sym == XKB_KEY_underscore ||
    sym == XKB_KEY_0 || sym == XKB_KEY_9 ||
    sym == XKB_KEY_Delete;

  if (regularControl) {
    resp.blockClientDelivery = true;
    resp.consumed = true;
    return resp;
  }

  return resp;
}

bool
Controls::matchesConfiguredKey(xkb_keysym_t sym, const std::string fn)
{
  int mapped = controlMappings.getKey(fn);
  return mapped != -1 && sym == static_cast<xkb_keysym_t>(mapped);
}

void
Controls::enqueueAction(std::function<void()> fn)
{
  std::lock_guard<std::mutex> lock(queuedActionsMutex);
  queuedActions.push_back(std::move(fn));
}

void
Controls::runQueuedActions()
{
  std::vector<std::function<void()>> actions;
  {
    std::lock_guard<std::mutex> lock(queuedActionsMutex);
    actions.swap(queuedActions);
  }
  for (auto& fn : actions) {
    if (fn) {
      fn();
    }
  }
}

void
Controls::applyMovementInput(bool forward, bool back, bool left, bool right)
{
  if (camera) {
    camera->handleTranslateForce(forward, back, left, right, false, false);
  }
}

void
Controls::clearMovementInput()
{
  applyMovementInput(false, false, false, false);
}

void
Controls::applyLookDelta(double dx, double dy)
{
  if (renderer && renderer->getCamera()) {
    renderer->getCamera()->handleRotateForce(nullptr, dx, dy);
  }
}

bool
Controls::isPressed(xkb_keysym_t sym) const
{
  return pressed.count(sym) > 0;
}

bool
Controls::isPressedEither(xkb_keysym_t a, xkb_keysym_t b) const
{
  return isPressed(a) || isPressed(b);
}

void
Controls::handleToggleApp()
{
  auto app = windowManagerSpace ? windowManagerSpace->getLookedAtApp()
                                : std::optional<entt::entity>();
  if (app.has_value() && isPressedEither(XKB_KEY_r, XKB_KEY_R) &&
      debounce(lastKeyPressTime)) {
    goToApp(app.value());
  }
}

bool
Controls::handleWaylandHotkeys()
{
  if (!waylandModifierHeld || !wm) {
    return false;
  }

  if (isPressedEither(XKB_KEY_p, XKB_KEY_P) && debounce(lastKeyPressTime)) {
    wm->requestScreenshot();
    return true;
  }

  if (is_configured_or_fallback_pressed(
        controlMappings, pressed, "unfocus_app", XKB_KEY_e, XKB_KEY_E) &&
      debounce(lastKeyPressTime)) {
    wm->unfocusApp();
    return true;
  }

  if (isPressedEither(XKB_KEY_q, XKB_KEY_Q) && debounce(lastKeyPressTime)) {
    if (wm->getCurrentlyFocusedApp().has_value()) {
      if (auto* comp = registry->try_get<WaylandApp::Component>(
            wm->getCurrentlyFocusedApp().value())) {
        if (comp->app) {
          comp->app->close();
        }
      }
    }
    wm->unfocusApp();
    return true;
  }

  if (isPressed(XKB_KEY_0) && debounce(lastKeyPressTime)) {
    wm->unfocusApp();
    moveTo(glm::vec3(3.0, 5.0, 16), std::nullopt, 4);
    return true;
  }

  for (int i = 0; i < 9; ++i) {
    xkb_keysym_t digit = static_cast<xkb_keysym_t>(XKB_KEY_1 + i);
    xkb_keysym_t shifted = normalize_hotkey_sym(digit);
    switch (digit) {
      case XKB_KEY_1:
        shifted = XKB_KEY_exclam;
        break;
      case XKB_KEY_2:
        shifted = XKB_KEY_at;
        break;
      case XKB_KEY_3:
        shifted = XKB_KEY_numbersign;
        break;
      case XKB_KEY_4:
        shifted = XKB_KEY_dollar;
        break;
      case XKB_KEY_5:
        shifted = XKB_KEY_percent;
        break;
      case XKB_KEY_6:
        shifted = XKB_KEY_asciicircum;
        break;
      case XKB_KEY_7:
        shifted = XKB_KEY_ampersand;
        break;
      case XKB_KEY_8:
        shifted = XKB_KEY_asterisk;
        break;
      case XKB_KEY_9:
        shifted = XKB_KEY_parenleft;
        break;
      default:
        break;
    }
    if (!isPressed(digit) && !isPressed(shifted)) {
      continue;
    }
    if (!debounce(lastKeyPressTime)) {
      return true;
    }
    log_controls("number hotkey: %d", i);
    if (waylandShiftHeld) {
      if (wm->getCurrentlyFocusedApp().has_value()) {
        int source = wm->findAppsHotKey(wm->getCurrentlyFocusedApp().value());
        wm->swapHotKeys(source, i);
      }
    } else if (auto ent = wm->getHotkeyTarget(i)) {
      wm->unfocusApp();
      goToApp(*ent);
    }
    return true;
  }

  return false;
}
