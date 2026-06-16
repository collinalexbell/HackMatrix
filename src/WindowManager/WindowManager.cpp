#include "WindowManager/WindowManager.h"
#include "components/Bootable.h"
#include "controls.h"
#include "entity.h"
#include "renderer.h"
#include "screen.h"
#include "systems/Boot.h"
#include "Config.h"
#include "model.h"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iostream>

#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>
#include <spdlog/common.h>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <cstdlib>
#include <sys/wait.h>
#include <sys/stat.h>

#ifndef WLROOTS_DEBUG_LOGS
#define WLROOTS_DEBUG_LOGS
#endif

#ifdef WLROOTS_DEBUG_LOGS
constexpr bool kWlrootsDebugLogs = true;
#else
constexpr bool kWlrootsDebugLogs = false;
#endif

#ifdef WLROOTS_DEBUG_LOGS
static FILE*
wlroots_wm_log()
{
  static FILE* f = []() {
    FILE* f2 = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
    return f2 ? f2 : stderr;
  }();
  return f;
}
#define WL_WM_LOG(...)                                                            \
  do {                                                                            \
    FILE* f = wlroots_wm_log();                                                   \
    if (f) {                                                                      \
      std::fprintf(f, __VA_ARGS__);                                               \
      std::fflush(f);                                                             \
    }                                                                             \
  } while (0)
#else
#define WL_WM_LOG(...) do { } while (0)
#endif

#define OBS false
#define EDGE false
#define TERM false
// todo: Magica still doesn't work with dynamic loading
#define MAGICA false

extern char** environ;

namespace WindowManager {

namespace {

const char*
getEnv(const char* name, char** envp)
{
  if (envp) {
    size_t nlen = std::strlen(name);
    for (char** p = envp; *p; ++p) {
      if (std::strncmp(*p, name, nlen) == 0 && (*p)[nlen] == '=') {
        return *p + nlen + 1;
      }
    }
  }
  return std::getenv(name);
}

static std::string
parseInlineEnvXdg(const std::string& program)
{
  std::istringstream iss(program);
  std::string token;
  while (iss >> token) {
    auto eq = token.find('=');
    if (eq != std::string::npos && eq > 0) {
      auto key = token.substr(0, eq);
      auto val = token.substr(eq + 1);
      if (key == "XDG_RUNTIME_DIR") {
        return val;
      }
      // Continue scanning inline exports until we hit a non-assignment.
    } else {
      break;
    }
  }
  return "";
}

static void
configureWaylandChildEnvironment(const std::string& waylandDisplay,
                                 const std::string& runtimeDir)
{
  if (!waylandDisplay.empty()) {
    setenv("WAYLAND_DISPLAY", waylandDisplay.c_str(), 1);
  }
  if (!runtimeDir.empty()) {
    setenv("XDG_RUNTIME_DIR", runtimeDir.c_str(), 1);
  }
  if (const char* xwaylandDisplay = std::getenv("HACKMATRIX_DISPLAY")) {
    setenv("DISPLAY", xwaylandDisplay, 1);
  } else {
    unsetenv("DISPLAY");
  }
  setenv("XDG_SESSION_TYPE", "wayland", 1);
  setenv("GDK_BACKEND", "wayland,x11", 1);
  setenv("QT_QPA_PLATFORM", "wayland;xcb", 1);
  setenv("SDL_VIDEODRIVER", "wayland,x11", 1);
  setenv("CLUTTER_BACKEND", "wayland", 1);
  setenv("ELM_DISPLAY", "wl", 1);
  setenv("MOZ_ENABLE_WAYLAND", "1", 1);
  // Chromium/Electron need an explicit native Wayland preference.
  setenv("OZONE_PLATFORM", "wayland", 1);
  setenv("ELECTRON_OZONE_PLATFORM_HINT", "wayland", 1);
}

static void
launchNestedProgram(const std::string& program, const char* logLabel)
{
  std::string runtimeDir = parseInlineEnvXdg(program);
  std::string waylandDisplay = getEnv("HACKMATRIX_WAYLAND_DISPLAY", environ);
  if (waylandDisplay.empty()) {
    waylandDisplay = getEnv("WAYLAND_DISPLAY", environ);
  }
  if (runtimeDir.empty()) {
    const char* envVal = getEnv("XDG_RUNTIME_DIR", environ);
    if (envVal) {
      runtimeDir = envVal;
    }
  }
  if (!runtimeDir.empty()) {
    struct stat st;
    if (stat(runtimeDir.c_str(), &st) != 0) {
      mkdir(runtimeDir.c_str(), 0700);
    }
  }

  std::thread([program, runtimeDir, waylandDisplay, logLabel] {
    pid_t pid = fork();
    if (pid == 0) {
      setsid();
      configureWaylandChildEnvironment(waylandDisplay, runtimeDir);
      WL_WM_LOG("WM: launching %s with WAYLAND_DISPLAY=%s DISPLAY=%s XDG_RUNTIME_DIR=%s\n",
                logLabel,
                std::getenv("WAYLAND_DISPLAY") ? std::getenv("WAYLAND_DISPLAY") : "(null)",
                std::getenv("DISPLAY") ? std::getenv("DISPLAY") : "(null)",
                std::getenv("XDG_RUNTIME_DIR") ? std::getenv("XDG_RUNTIME_DIR") : "(null)");
      execl("/bin/sh", "sh", "-c", program.c_str(), (char*)nullptr);
      _exit(127);
    }
    int status = 0;
    if (pid > 0) {
      waitpid(pid, &status, 0);
    }
  }).detach();
}

}

void WindowManager::menu() {
  launchNestedProgram(menuProgram, "menu");
}

void WindowManager::launchTerminal() {
  if (terminalProgram.empty()) {
    return;
  }
  launchNestedProgram(terminalProgram, "terminal");
}

void WindowManager::createAndRegisterApps(char **envp) {
  // Native Wayland surfaces register when they map through the compositor.
}

void
WindowManager::wire(WindowManagerPtr sharedThis,
                    Camera* camera,
                    Renderer* renderer)
{
  space = make_shared<Space>(registry, renderer, camera, logSink);
  renderer->wireWindowManager(sharedThis, space);
  if (controls) {
    controls->wireWindowManager(space);
  }
  this->renderer = renderer;
  this->camera = camera;
}

bool
WindowManager::hasCurrentOrPendingFocus()
{
  auto isValidFocus = [this](const std::optional<entt::entity>& ent) {
    return ent.has_value() && registry && registry->valid(*ent);
  };

  if (isValidFocus(pendingFocusedApp)) {
    return true;
  }
  if (pendingFocusedApp.has_value()) {
    pendingFocusedApp = std::nullopt;
  }

  if (isValidFocus(currentlyFocusedApp)) {
    return true;
  }
  if (currentlyFocusedApp.has_value()) {
    currentlyFocusedApp = std::nullopt;
  }

  if (isValidFocus(cursorInputFocusedApp)) {
    return true;
  }
  if (cursorInputFocusedApp.has_value()) {
    cursorInputFocusedApp = std::nullopt;
  }

  return false;
}

optional<entt::entity> WindowManager::getCurrentlyFocusedApp() {
  if (currentlyFocusedApp && (!registry || !registry->valid(*currentlyFocusedApp))) {
    currentlyFocusedApp = std::nullopt;
  }
  return currentlyFocusedApp;
}

optional<entt::entity>
WindowManager::getPendingFocusedApp()
{
  if (pendingFocusedApp && (!registry || !registry->valid(*pendingFocusedApp))) {
    pendingFocusedApp = std::nullopt;
  }
  return pendingFocusedApp;
}

optional<entt::entity>
WindowManager::getCursorInputFocusedApp()
{
  if (cursorInputFocusedApp &&
      (!registry || !registry->valid(*cursorInputFocusedApp))) {
    cursorInputFocusedApp = std::nullopt;
  }
  return cursorInputFocusedApp;
}

bool WindowManager::computeAppCameraTarget(entt::entity ent,
                                           glm::vec3& targetPos,
                                           glm::vec3& rotationDegrees,
                                           const char* reasonTag) {
  if (!space || !camera || !registry || !registry->all_of<Positionable>(ent)) {
    return false;
  }
  float deltaZ = space->getViewDistanceForWindowSize(ent);
  rotationDegrees = space->getAppRotation(ent);
  glm::quat rotationQuat = glm::quat(glm::radians(rotationDegrees));

  glm::vec3 appPos = space->getAppPosition(ent);
  targetPos = appPos + rotationQuat * glm::vec3(0, 0, deltaZ);

  WL_WM_LOG("WM: %s ent=%d pos=(%.2f,%.2f,%.2f) camPos=(%.2f,%.2f,%.2f) targetPos=(%.2f,%.2f,%.2f)\n",
            reasonTag ? reasonTag : "computeAppCameraTarget",
            (int)entt::to_integral(ent),
            appPos.x, appPos.y, appPos.z,
            camera->position.x, camera->position.y, camera->position.z,
            targetPos.x, targetPos.y, targetPos.z);
  return true;
}

std::shared_ptr<bool> WindowManager::moveCameraToApp(entt::entity ent, const char* reasonTag) {
  glm::vec3 targetPos = camera ? camera->position : glm::vec3{};
  glm::vec3 rotationDegrees{0.0f};
  bool haveTarget = computeAppCameraTarget(ent, targetPos, rotationDegrees, reasonTag);
  if (!haveTarget || !camera) {
    return {};
  }
  glm::quat rotationQuat = glm::quat(glm::radians(rotationDegrees));
  glm::vec3 facing = rotationQuat * glm::vec3(0, 0, -1);
  return camera->moveTo(targetPos, facing, 0.5f);
}

void WindowManager::focusEntityAfterMove(entt::entity ent) {
  if (!registry || !registry->valid(ent)) {
    return;
  }
  pendingFocusedApp = std::nullopt;
  WL_WM_LOG("WM: focusEntityAfterMove ent=%d isWayland=%d\n",
            (int)entt::to_integral(ent),
            registry->all_of<WaylandApp::Component>(ent) ? 1 : 0);
  if (registry->all_of<WaylandApp::Component>(ent)) {
    currentlyFocusedApp = ent;
    if (controls) {
      // Drop any lingering movement so movement keys don't stay "held" when focus
      // shifts to a Wayland client.
      controls->clearMovementInput();
    }
    if (auto* comp = registry->try_get<WaylandApp::Component>(ent)) {
      if (comp->app) {
        comp->app->takeInputFocus();
      }
    }
  }
}

void WindowManager::swapHotKeys(int a, int b) {
  if (a < 0 || b < 0) {
    return;
  }
  if (a >= static_cast<int>(appsWithHotKeys.size()) ||
      b >= static_cast<int>(appsWithHotKeys.size())) {
    return;
  }

  std::lock_guard<std::mutex> lock(renderLoopMutex);
  std::swap(appsWithHotKeys[a], appsWithHotKeys[b]);
}

void WindowManager::assignHotkeySlot(entt::entity ent)
{
  // Reuse the first empty fixed slot.
  for (auto& opt : appsWithHotKeys) {
    if (!opt.has_value()) {
      opt = ent;
      return;
    }
  }
}

void WindowManager::releaseHotkeySlot(entt::entity theApp)
{
  for (auto& slot : appsWithHotKeys) {
    if (slot.has_value() && slot.value() == theApp) {
      slot = nullopt;
      return;
    }
  }
}

int WindowManager::findAppsHotKey(entt::entity theApp)
{
  for (size_t i = 0; i < appsWithHotKeys.size(); i++) {
    if (appsWithHotKeys[i].has_value() &&
        appsWithHotKeys[i].value() == theApp) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

optional<entt::entity>
WindowManager::getHotkeyTarget(int index)
{
  if (index < 0 || index >= static_cast<int>(appsWithHotKeys.size())) {
    return nullopt;
  }
  auto& slot = appsWithHotKeys[index];
  if (!slot.has_value()) {
    return nullopt;
  }
  if (!registry || !registry->valid(*slot) ||
      !registry->all_of<Positionable>(*slot)) {
    slot = nullopt;
    return nullopt;
  }
  return slot;
}


void
WindowManager::focusApp(entt::entity appEntity)
{
  if (!registry || !registry->valid(appEntity)) {
    currentlyFocusedApp = std::nullopt;
    pendingFocusedApp = std::nullopt;
    cursorInputFocusedApp = std::nullopt;
    return;
  }
  // Wayland focus is handled via wlroots; record focus only.
  pendingFocusedApp = std::nullopt;
  cursorInputFocusedApp = std::nullopt;
  currentlyFocusedApp = appEntity;
  // Clear any stuck movement when compositor focus moves to a client.
  if (controls) {
    controls->clearMovementInput();
  }
  if (registry->valid(appEntity) &&
      registry->all_of<WaylandApp::Component>(appEntity)) {
    // Notify the Wayland client so keyboard/pointer focus matches click focus.
    if (auto* comp = registry->try_get<WaylandApp::Component>(appEntity)) {
      comp->app->takeInputFocus();
    }
  }
}

void
WindowManager::setCursorInputFocus(entt::entity appEntity)
{
  if (!registry || !registry->valid(appEntity)) {
    clearCursorInputFocus();
    return;
  }
  if (currentlyFocusedApp && *currentlyFocusedApp == appEntity) {
    cursorInputFocusedApp = std::nullopt;
    return;
  }
  if (cursorInputFocusedApp && *cursorInputFocusedApp == appEntity) {
    return;
  }
  clearCursorInputFocus();
  cursorInputFocusedApp = appEntity;
  if (registry->all_of<WaylandApp::Component>(appEntity)) {
    if (auto* comp = registry->try_get<WaylandApp::Component>(appEntity)) {
      if (comp->app) {
        comp->app->takeInputFocus();
      }
    }
  }
}

void
WindowManager::clearCursorInputFocus()
{
  if (!cursorInputFocusedApp.has_value()) {
    return;
  }
  auto ent = *cursorInputFocusedApp;
  cursorInputFocusedApp = std::nullopt;
  if (!registry || !registry->valid(ent)) {
    return;
  }
  if (currentlyFocusedApp && *currentlyFocusedApp == ent) {
    return;
  }
  if (registry->all_of<WaylandApp::Component>(ent)) {
    auto& appComponent = registry->get<WaylandApp::Component>(ent);
    if (appComponent.app) {
      appComponent.app->unfocus();
    }
  }
}

void WindowManager::unfocusApp() {
  if (logger) {
    logger->debug("unfocusing app");
  }
  if (!currentlyFocusedApp.has_value()) {
    clearCursorInputFocus();
    return;
  }
  auto ent = currentlyFocusedApp.value();
  if (!registry || !registry->valid(ent)) {
    currentlyFocusedApp = std::nullopt;
    return;
  }
  if (registry->all_of<WaylandApp::Component>(ent)) {
    auto& appComponent = registry->get<WaylandApp::Component>(ent);
    appComponent.app->unfocus();
  }
  currentlyFocusedApp = std::nullopt;
  pendingFocusedApp = std::nullopt;
  cursorInputFocusedApp = std::nullopt;
  WL_WM_LOG("WM: unfocusApp ent=%d\n", (int)entt::to_integral(ent));
}


bool
WindowManager::computeFocusedSpawn(entt::entity newApp, glm::vec3& pos, glm::vec3& rot) const
{
  if (!registry || !currentlyFocusedApp || !registry->valid(*currentlyFocusedApp)) {
    return false;
  }
  if (!camera) {
    return false;
  }
  if (!registry->all_of<Positionable>(*currentlyFocusedApp)) {
    return false;
  }
  auto& focusPos = registry->get<Positionable>(*currentlyFocusedApp);
  // Match the camera's focus distance for the new window size.
  float focusedSpawnOffset = 1.2f;
  if (space && registry->valid(newApp)) {
    focusedSpawnOffset =
      std::max(0.1f, space->getViewDistanceForWindowSize(newApp));
  }
  focusedSpawnOffset *= 1.2f;
  rot = focusPos.rotate;
  glm::quat focusRotation = glm::quat(glm::radians(rot));
  glm::vec3 localOffset(focusedSpawnOffset, 0.0f, 0.0f);
  glm::vec3 rotatedOffset = focusRotation * localOffset;
  pos = focusPos.pos + rotatedOffset;
  return true;
}

void
WindowManager::positionRelativeToFocus(entt::entity appEntity)
{
  if (!registry || !registry->valid(appEntity) || !registry->all_of<Positionable>(appEntity)) {
    return;
  }
  glm::vec3 pos;
  glm::vec3 rot;
  if (!computeFocusedSpawn(appEntity, pos, rot)) {
    return;
  }
  auto& positionable = registry->get<Positionable>(appEntity);
  positionable.pos = pos;
  positionable.rotate = rot;
  positionable.damage();
}

void
WindowManager::setCursorVisible(bool visible)
{
  cursorVisible = visible;
}

void
WindowManager::requestScreenshot()
{
  screenshotRequested = true;
  WL_WM_LOG("WM: screenshot requested\n");
}

bool
WindowManager::consumeScreenshotRequest()
{
  bool expected = true;
  if (screenshotRequested.compare_exchange_strong(expected, false)) {
    return true;
  }
  return false;
}

void WindowManager::registerControls(Controls *controls) {
  this->controls = controls;
}

void WindowManager::setupLogger() {
  if (!logSink) {
    return;
  }
  logger = make_shared<spdlog::logger>("wm", logSink);
  logger->set_level(spdlog::level::debug);
  logger->flush_on(spdlog::level::info);
  logger->debug("WindowManager()");
}

WindowManager::WindowManager(shared_ptr<EntityRegistry> registry,
                             spdlog::sink_ptr loggerSink,
                             char** envp)
  : logSink(loggerSink)
  , registry(registry)
  , envp(envp)
{
  menuProgram = Config::singleton()->get<std::string>("menu_program");
  try {
    terminalProgram =
      Config::singleton()->get<std::string>("scriptable_terminal_program");
  } catch (...) {
    terminalProgram.clear();
  }
  if (const char* envMenu = std::getenv("MENU_PROGRAM")) {
    menuProgram = envMenu;
  }
  if (const char* envTerminal = std::getenv("TERMINAL_PROGRAM")) {
    terminalProgram = envTerminal;
  }
  setupLogger();
}

WindowManager::~WindowManager() {
  {
    lock_guard<std::mutex> continueLock(continueMutex);
    continueRunning = false;
  }
  if(substructureThread.joinable()) {
    substructureThread.join();
  }
  systems::killBootablesOnExit(registry);
}

entt::entity WindowManager::registerWaylandApp(std::shared_ptr<WaylandApp> app,
                                               bool spawnAtCamera,
                                               bool accessory,
                                               entt::entity parent,
                                               int offsetX,
                                               int offsetY,
                                               bool layerShell,
                                               int screenX,
                                               int screenY,
                                               int screenW,
                                               int screenH) {
  if (!app || !registry) {
    return entt::null;
  }
  WL_WM_LOG("WM: registerWaylandApp size=%dx%d spawnAtCamera=%d accessory=%d parent=%d offset=(%d,%d)\n",
            app->getWidth(),
            app->getHeight(),
            spawnAtCamera ? 1 : 0,
            accessory ? 1 : 0,
            parent == entt::null ? -1 : (int)entt::to_integral(parent),
            offsetX,
            offsetY);
  entt::entity entity = registry->create();
  registry->emplace<WaylandApp::Component>(
    entity, app, accessory, layerShell, parent, offsetX, offsetY, screenX, screenY, screenW, screenH);
  if (auto* comp = registry->try_get<WaylandApp::Component>(entity)) {
    if (comp->screen_w == 0) {
      comp->screen_w = static_cast<int>(SCREEN_WIDTH);
    }
    if (comp->screen_h == 0) {
      comp->screen_h = static_cast<int>(SCREEN_HEIGHT);
    }
  }
  // Attach textures immediately so layer shells/popups can be blitted directly.
  if (renderer) {
    renderer->registerApp(app.get());
  } else {
    WL_WM_LOG("WM: renderer missing; registered component only for entity=%d\n",
              (int)entt::to_integral(entity));
  }

  // Layer-shell menus should take focus immediately so they receive keystrokes.
  if (layerShell) {
    auto focused = getCurrentlyFocusedApp();
    // bool allowFocus = (action.parent_surface != nullptr) ||
    // action.menu_surface;
    unfocusApp();
    focusApp(entity);
  }

  // Accessory apps (e.g. popups/menus) should not be positionable or bound to
  // hotkeys; they are rendered relative to their parent.
  if (accessory) {
    return entity;
  }

  // Place in world space similar to spawnAtCamera path.
  glm::vec3 pos(0.0f, 3.5f, -2.0f);
  glm::vec3 rot(0.0f);
  bool placedFromFocus = computeFocusedSpawn(entity, pos, rot);
  if (!placedFromFocus && spawnAtCamera && camera) {
    float dist = 2.0f;
    if (space && registry->valid(entity)) {
      dist = std::max(0.1f, space->getViewDistanceForWindowSize(entity));
    }
    float yaw = camera->getYaw();
    float pitch = camera->getPitch();
    glm::quat yawRotation =
      glm::angleAxis(glm::radians(90 + yaw), glm::vec3(0.0f, -1.0f, 0.0f));
    glm::quat pitchRotation =
      glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat finalRotation = yawRotation * pitchRotation;
    rot = glm::degrees(glm::eulerAngles(finalRotation));
    pos = camera->position + finalRotation * glm::vec3(0, 0, -dist);
  }
  registry->emplace<Positionable>(entity, pos, glm::vec3(0.0f), rot, 1.0f);
  // Mark as damaged so renderer updates transforms.
  if (auto* p = registry->try_get<Positionable>(entity)) {
    p->damage();
  }
  WL_WM_LOG("WM: WaylandApp entity=%d size=%dx%d pos=(%.2f, %.2f, %.2f)\n",
            (int)entt::to_integral(entity),
            app->getWidth(),
            app->getHeight(),
            pos.x,
            pos.y,
            pos.z);
  assignHotkeySlot(entity);
  return entity;
}

} // namespace WindowManager
#ifndef WLROOTS_DEBUG_LOGS
#define WLROOTS_DEBUG_LOGS
#endif
