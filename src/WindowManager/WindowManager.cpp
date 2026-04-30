#include "WindowManager/WindowManager.h"
#include "app.h"
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

  // Force toolkit stacks to prefer the nested HackMatrix Wayland session.
  // Keep DISPLAY inherited so XWayland clients can still fall back inside
  // HackMatrix instead of leaking to the parent desktop.
  setenv("XDG_SESSION_TYPE", "wayland", 1);
  setenv("GDK_BACKEND", "wayland,x11", 1);
  setenv("QT_QPA_PLATFORM", "wayland;xcb", 1);
  setenv("SDL_VIDEODRIVER", "wayland,x11", 1);
  setenv("CLUTTER_BACKEND", "wayland", 1);
  setenv("ELM_DISPLAY", "wl", 1);
  setenv("MOZ_ENABLE_WAYLAND", "1", 1);
}

static unsigned int
resolveHotkeyMaskFromConfig()
{
  std::string mod = "alt";
  try {
    mod = Config::singleton()->get<std::string>("key_mappings.super_modifier");
  } catch (...) {
    // Default to "alt" when not provided.
  }
  std::transform(mod.begin(), mod.end(), mod.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  if (mod == "alt" || mod == "mod1" || mod == "option") {
    return Mod1Mask;
  }
  // Fallback to the traditional Super/Logo mapping.
  return Mod4Mask;
}

}

void WindowManager::menu() {
  auto program = menuProgram;
  std::string runtimeDir = parseInlineEnvXdg(program);
  std::string waylandDisplay = getEnv("WAYLAND_DISPLAY", environ);
  if (runtimeDir.empty()) {
    const char* envVal = getEnv("XDG_RUNTIME_DIR", environ);
    if (envVal) {
      runtimeDir = envVal;
    }
  }
  if (!runtimeDir.empty()) {
    struct stat st;
    if (stat(runtimeDir.c_str(), &st) != 0) {
      int mk = mkdir(runtimeDir.c_str(), 0700);
    }
  }
  std::thread([program, runtimeDir, waylandDisplay] {
    pid_t pid = fork();
    if (pid == 0) {
      setsid();
      configureWaylandChildEnvironment(waylandDisplay, runtimeDir);
      WL_WM_LOG("WM: launching menu with WAYLAND_DISPLAY=%s XDG_RUNTIME_DIR=%s DISPLAY=%s\n",
                std::getenv("WAYLAND_DISPLAY") ? std::getenv("WAYLAND_DISPLAY") : "(null)",
                std::getenv("XDG_RUNTIME_DIR") ? std::getenv("XDG_RUNTIME_DIR") : "(null)",
                std::getenv("DISPLAY") ? std::getenv("DISPLAY") : "(null)");
      execl("/bin/sh", "sh", "-c", program.c_str(), (char*)nullptr);
      // execl only returns on failure.
      _exit(127);
    }
    int status = 0;
    if (pid > 0) {
      waitpid(pid, &status, 0);
    }
  }).detach();
}

void WindowManager::createAndRegisterApps(char **envp) {
  if (waylandMode) {
    // TODO: Fix, we need the bootable system
    return;
  }
  logger->info("enter createAndRegisterApps()");
  auto alreadyBooted = systems::getAlreadyBooted(registry);
  for(auto entityAndPid : alreadyBooted) {
    auto bootable = registry->get<Bootable>(entityAndPid.first);
    assignHotkeySlot(entityAndPid.first);
    auto app = X11App::byPID(entityAndPid.second, display, screen,
                             bootable.getWidth(), bootable.getHeight());
    addApp(app, entityAndPid.first);
  }
  systems::bootAll(registry, envp);

  //forkOrFindApp("/usr/bin/emacs", "emacs", "Emacs", ideSelection.emacs, envp);
  //forkOrFindApp("/usr/bin/code", "emacs", "Emacs", ideSelection.vsCode, envp);
  //killTerminator();
  logger->info("exit createAndRegisterApps()");
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
  auto rv = false;
  std::optional<entt::entity> focusCandidate = getPendingFocusedApp();
  if (!focusCandidate) {
    focusCandidate = getCurrentlyFocusedApp();
  }
  if (focusCandidate) {
    rv = true;
  }
  return rv;
}

optional<entt::entity> WindowManager::getCurrentlyFocusedApp() {
    return currentlyFocusedApp;
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
  WL_WM_LOG("WM: focusEntityAfterMove ent=%d isWayland=%d isX11=%d\n",
            (int)entt::to_integral(ent),
            registry->all_of<WaylandApp::Component>(ent) ? 1 : 0,
            registry->all_of<X11App>(ent) ? 1 : 0);
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
  } else if (registry->all_of<X11App>(ent)) {
    focusApp(ent);
  }
}

void WindowManager::addApp(X11App *app, entt::entity entity) {
  std::lock_guard<std::mutex> lock(renderLoopMutex);
  auto window = app->getWindow();
  appsToAdd.push_back(app);
  dynamicApps[window] = entity;
}

void WindowManager::createApp(Window window, unsigned int width,
                                 unsigned int height) {

  X11App *app = X11App::byWindow(window, display, screen, width, height);

  // some applications grab focus on boot
  if(!app->isAccessory()) {
    app->unfocus(matrix);
  }

  if(currentlyFocusedApp.has_value()) {
    auto& app = registry->get<X11App>(currentlyFocusedApp.value());
    app.focus(matrix);
  }

  entt::entity entity;

  auto bootableEntity = systems::matchApp(registry, app);

  if(bootableEntity.has_value()) {
    entity = bootableEntity.value();
  } else {
    entity = registry->create();
  }
  if(!app->isAccessory()) {
    assignHotkeySlot(entity);
  }
  addApp(app, entity);
}

void WindowManager::onMapRequest(XMapRequestEvent event) {
  bool alreadyRegistered = dynamicApps.count(event.window);

  stringstream ss;
  ss << "map request for window: " << event.window << ", "
     << "alreadyRegistered: " << alreadyRegistered;
  logger->debug(ss.str());
  logger->flush();

  if (!alreadyRegistered) {
    createApp(event.window);
  }
}

void WindowManager::compactHotkeyList()
{
  // Trim trailing empty slots to keep indices dense.
  while (!appsWithHotKeys.empty() && !appsWithHotKeys.back().has_value()) {
    appsWithHotKeys.pop_back();
  }
}

void WindowManager::removeAppForWindow(Window window) {
  renderLoopMutex.lock();
  if (dynamicApps.contains(window)) {
    auto appEntity = dynamicApps.at(window);
    dynamicApps.erase(window);
    auto hotkey = find(appsWithHotKeys.begin(), appsWithHotKeys.end(), appEntity);
    if(hotkey != appsWithHotKeys.end()) {
      appsWithHotKeys.erase(hotkey);
      compactHotkeyList();
    }
    appsToRemove.push_back(appEntity);
  }
  renderLoopMutex.unlock();
}

void WindowManager::swapHotKeys(int a, int b) {
  if (a < 0 || b < 0) {
    return;
  }

  std::lock_guard<std::mutex> lock(renderLoopMutex);
  size_t requiredSize = static_cast<size_t>(std::max(a, b) + 1);
  if (appsWithHotKeys.size() < requiredSize) {
    appsWithHotKeys.resize(requiredSize);
  }

  std::swap(appsWithHotKeys[a], appsWithHotKeys[b]);
  compactHotkeyList();
}

void WindowManager::assignHotkeySlot(entt::entity ent)
{
  // Reuse the first empty slot if one exists; otherwise append.
  for (auto& opt : appsWithHotKeys) {
    if (!opt.has_value()) {
      opt = ent;
      compactHotkeyList();
      return;
    }
  }
  appsWithHotKeys.push_back(ent);
  compactHotkeyList();
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
    compactHotkeyList();
    return nullopt;
  }
  return slot;
}


void WindowManager::reconfigureWindow(XConfigureEvent configureEvent) {
  if (waylandMode) {
    return;
  }
  if(dynamicApps.contains(configureEvent.window)) {
    auto app = registry->try_get<X11App>(dynamicApps[configureEvent.window]);
    if(app!=NULL) {
      app->resize(configureEvent.width, configureEvent.height);
    }
  }
}

void WindowManager::createUnfocusHackThread(entt::entity entity) {
  auto app = registry->try_get<X11App>(entity);
  try {
    if (app != NULL && !app->isAccessory() &&
        !currentlyFocusedApp.has_value()) {
      auto t = thread([this, entity]() -> void {
        auto app = registry->try_get<X11App>(entity);
        usleep(0.5 * 1000000);
        if (app != NULL) {
          app->unfocus(matrix);
        }
      });
      t.detach();
    }
  } catch (...) {
    logger->error("likely it was the app->isAccessory");
  }
}

void WindowManager::logWaitForRemovalChangeSize(int changeSize) {
    if (changeSize < 0) {
      logger->info("waitForRemovalCount decreased");
    }
    if (changeSize > 0){
      logger->info("waitForRemovalCount increased");
    }
}

  int WindowManager::waitForRemovalChangeSize(int curSize) {
  static int lastWaitForRemovalCount = 0;
  auto changeSize = curSize - lastWaitForRemovalCount;
  lastWaitForRemovalCount = changeSize;
  return changeSize;
}

void WindowManager::adjustAppsToAddAfterAdditions(vector<X11App*> &waitForRemoval) {
  appsToAdd.clear();
  int changeSize = waitForRemovalChangeSize(waitForRemoval.size());
  logWaitForRemovalChangeSize(changeSize);
  if(waitForRemoval.size() >= 10) {
    logger->critical("waitForRemoval size is critically large");
  }
  appsToAdd.assign(waitForRemoval.begin(), waitForRemoval.end());
}

void
WindowManager::focusApp(entt::entity appEntity)
{
  if (!registry || !registry->valid(appEntity)) {
    currentlyFocusedApp = std::nullopt;
    pendingFocusedApp = std::nullopt;
    return;
  }
  // Wayland focus is handled via wlroots; record focus only.
  pendingFocusedApp = std::nullopt;
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

void WindowManager::unfocusApp() {
  if (logger) {
    logger->debug("unfocusing app");
  }
  if (!currentlyFocusedApp.has_value()) {
    return;
  }
  auto ent = currentlyFocusedApp.value();
  if (!registry || !registry->valid(ent)) {
    currentlyFocusedApp = std::nullopt;
    return;
  }
  if (registry->all_of<WaylandApp::Component>(ent)) {
    auto& appComponent = registry->get<WaylandApp::Component>(ent);
    appComponent.app->unfocus(matrix);
  } else if (registry->all_of<X11App>(ent)) {
    auto& app = registry->get<X11App>(ent);
    app.unfocus(matrix);
  }
  currentlyFocusedApp = std::nullopt;
  pendingFocusedApp = std::nullopt;
  WL_WM_LOG("WM: unfocusApp (x11) ent=%d\n", (int)entt::to_integral(ent));
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
  float yaw = camera->getYaw();
  float pitch = camera->getPitch();
  glm::quat yawRotation =
    glm::angleAxis(glm::radians(90 + yaw), glm::vec3(0.0f, -1.0f, 0.0f));
  glm::quat pitchRotation =
    glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
  glm::quat finalRotation = yawRotation * pitchRotation;
  rot = focusPos.rotate;
  pos = focusPos.pos + glm::vec3(focusedSpawnOffset, 0.0f, 0.0f);
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
  if (waylandMode) {
    this->controls = controls;
    return;
  }
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
                             bool waylandMode,
                             char** envp)
  : logSink(loggerSink)
  , registry(registry)
  , waylandMode(waylandMode)
  , envp(envp)
{
  menuProgram = Config::singleton()->get<std::string>("menu_program");
  if (const char* envMenu = std::getenv("MENU_PROGRAM")) {
    menuProgram = envMenu;
  }
  hotkeyModifierMask = resolveHotkeyMaskFromConfig();
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
  if (!waylandMode && display) {
    XCompositeReleaseOverlayWindow(display, RootWindow(display, screen));
  }
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
