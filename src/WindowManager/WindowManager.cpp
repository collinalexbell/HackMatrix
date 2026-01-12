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
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
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

#include <X11/extensions/shape.h>
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

void
appendMenuLog(const std::string& line)
{
  FILE* f = std::fopen("/tmp/matrix-wlroots-menu.log", "a");
  if (!f) {
    return;
  }
  auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  std::fprintf(f, "[%lld] %s\n", static_cast<long long>(now), line.c_str());
  std::fclose(f);
}

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
  // Use current environment so we capture updated WAYLAND_DISPLAY/XDG_RUNTIME_DIR.
  char** envForChild = environ;
  std::string runtimeDir = parseInlineEnvXdg(program);
  std::string waylandDisplay = getEnv("WAYLAND_DISPLAY", envForChild);
  if (runtimeDir.empty()) {
    const char* envVal = getEnv("XDG_RUNTIME_DIR", envForChild);
    if (envVal) {
      runtimeDir = envVal;
    }
  }
  appendMenuLog("env WAYLAND_DISPLAY=" +
                (waylandDisplay.empty() ? std::string("(null)") : waylandDisplay) +
                " XDG_RUNTIME_DIR=" +
                (runtimeDir.empty() ? std::string("(null)") : runtimeDir));
  if (!runtimeDir.empty()) {
    struct stat st;
    if (stat(runtimeDir.c_str(), &st) != 0) {
      int mk = mkdir(runtimeDir.c_str(), 0700);
      if (mk != 0) {
        appendMenuLog("menu() could not create XDG_RUNTIME_DIR " + runtimeDir +
                      " errno=" + std::to_string(errno));
      } else {
        appendMenuLog("menu() created XDG_RUNTIME_DIR " + runtimeDir);
      }
    }
  }
  appendMenuLog("menu() launching: " + program);
  std::thread([program, envForChild, runtimeDir, waylandDisplay] {
    const std::string ioLog = "/tmp/matrix-wlroots-menu.stderr.log";
    int fd = ::open(ioLog.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
      appendMenuLog("menu() failed to open stderr log file");
      return;
    }
    pid_t pid = fork();
    if (pid == 0) {
      setsid();
      dup2(fd, STDOUT_FILENO);
      dup2(fd, STDERR_FILENO);
      close(fd);
      if (!waylandDisplay.empty()) {
        setenv("WAYLAND_DISPLAY", waylandDisplay.c_str(), 1);
      }
      if (!runtimeDir.empty()) {
        setenv("XDG_RUNTIME_DIR", runtimeDir.c_str(), 1);
      }
      execle("/bin/sh", "sh", "-c", program.c_str(), (char*)nullptr, envForChild);
      // execle only returns on failure.
      appendMenuLog("menu() exec failed, errno=" + std::to_string(errno));
      _exit(127);
    }
    close(fd);
    int status = 0;
    if (pid > 0) {
      waitpid(pid, &status, 0);
    }
    appendMenuLog("menu() exited rc=" + std::to_string(status));
  }).detach();
}

int forkApp(string cmd, char **envp, string args) {
  int pid = fork();
  if (pid == 0) {
    setsid();
    if (args != "") {
      execle(cmd.c_str(), cmd.c_str(), args.c_str(), NULL, envp);
    }
    execle(cmd.c_str(), cmd.c_str(), NULL, envp);
    exit(0);
  } else {
    return pid;
  }
}

void WindowManager::forkOrFindApp(string cmd, string pidOf, string className,
                                  entt::entity &appEntity, char **envp,
                                  string args) {
  char *line;
  std::size_t len = 0;
  FILE *pidPipe = popen(string("pgrep " + pidOf).c_str(), "r");
  if (getline(&line, &len, pidPipe) == -1) {
    forkApp(cmd, envp, args);
    if (className == "obs") {
      sleep(30);
    } else if (className == "magicavoxel.exe") {
      sleep(6);
    } else {
      sleep(4);
    }
  }
  X11App *app =
    X11App::byClass(className, display, screen,
                    Bootable::DEFAULT_WIDTH,
                    Bootable::DEFAULT_HEIGHT);
  appEntity = registry->create();
  registry->emplace<X11App>(appEntity, std::move(*app));
  dynamicApps[app->getWindow()] = appEntity;
  logger->info("created " + className + " app");
}

void WindowManager::createAndRegisterApps(char **envp) {
  if (waylandMode) {
    return;
  }
  logger->info("enter createAndRegisterApps()");
  auto alreadyBooted = systems::getAlreadyBooted(registry);
  for(auto entityAndPid : alreadyBooted) {
    auto bootable = registry->get<Bootable>(entityAndPid.first);
    appsWithHotKeys.push_back(entityAndPid.first);
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

void WindowManager::allow_input_passthrough(Window window) {
  if (waylandMode) {
    return;
  }
  XserverRegion region = XFixesCreateRegion(display, NULL, 0);

  XFixesSetWindowShapeRegion(display, window, ShapeBounding, 0, 0, 0);
  XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, region);

  XFixesDestroyRegion(display, region);
}

void WindowManager::passthroughInput() {
  if (waylandMode) {
    return;
  }
  allow_input_passthrough(overlay);
  allow_input_passthrough(matrix);
  XFlush(display);
}

void WindowManager::capture_input(Window window, bool shapeBounding,
                                  bool shapeInput) {
  if (waylandMode) {
    return;
  }
  // Create a region covering the entire window
  XserverRegion region = XFixesCreateRegion(display, NULL, 0);
  XRectangle rect;
  rect.x = 0;
  rect.y = 0;
  rect.width = SCREEN_WIDTH;  // Replace with your window's width
  rect.height = SCREEN_HEIGHT; // Replace with your window's height
  XFixesSetRegion(display, region, &rect, 1);

  if (shapeBounding) {
    XFixesSetWindowShapeRegion(display, window, ShapeBounding, 0, 0, region);
  }
  if (shapeInput) {
    XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, region);
  }
  XFixesDestroyRegion(display, region);
}

void WindowManager::captureInput() {
  if (waylandMode) {
    return;
  }
  capture_input(overlay, true, true);
  capture_input(matrix, true, true);
  XFlush(display);
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
  WL_WM_LOG("WM: focusEntityAfterMove ent=%d isWayland=%d isX11=%d\n",
            (int)entt::to_integral(ent),
            registry->all_of<WaylandApp::Component>(ent) ? 1 : 0,
            registry->all_of<X11App>(ent) ? 1 : 0);
  if (registry->all_of<WaylandApp::Component>(ent)) {
    currentlyFocusedApp = ent;
    if (auto* comp = registry->try_get<WaylandApp::Component>(ent)) {
      if (comp->app) {
        comp->app->takeInputFocus();
      }
    }
  } else if (registry->all_of<X11App>(ent)) {
    focusApp(ent);
  }
}

void WindowManager::goToLookedAtApp() {
  if (!space || !camera) {
    return;
  }
  auto looked = space->getLookedAtApp();
  if (!looked) {
    return;
  }
  moveCameraToApp(looked.value(), "goToLookedAtApp");
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
    appsWithHotKeys.push_back(entity);
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

void WindowManager::removeAppForWindow(Window window) {
  renderLoopMutex.lock();
  if (dynamicApps.contains(window)) {
    auto appEntity = dynamicApps.at(window);
    dynamicApps.erase(window);
    auto hotkey = find(appsWithHotKeys.begin(), appsWithHotKeys.end(), appEntity);
    if(hotkey != appsWithHotKeys.end()) {
      appsWithHotKeys.erase(hotkey);
    }
    appsToRemove.push_back(appEntity);
  }
  renderLoopMutex.unlock();
}

void WindowManager::swapHotKeys(int a, int b) {
  if(a < appsWithHotKeys.size() && b < appsWithHotKeys.size()) {
    auto aOpt = appsWithHotKeys[a];
    appsWithHotKeys[a] = appsWithHotKeys[b];
    appsWithHotKeys[b] = aOpt;
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

void WindowManager::handleHotkeySym(xkb_keysym_t sym, bool modifierHeld, bool shiftHeld)
{
  pruneInvalidFocus();
  auto focused = currentlyFocusedApp;

  // Screenshot hotkey works without modifiers to align with user-facing key.
  if (sym == XKB_KEY_p || sym == XKB_KEY_P) {
    requestScreenshot();
    return;
  }
  if (!modifierHeld) {
    return;
  }

  auto swapOrFocus = [&](int index) {
    if (shiftHeld && currentlyFocusedApp.has_value()) {
      int source = findAppsHotKey(currentlyFocusedApp.value());
      swapHotKeys(source, index);
      return;
    }
    unfocusApp();
    if (index >= 0 && index < static_cast<int>(appsWithHotKeys.size())) {
      if (!appsWithHotKeys[index].has_value()) {
        return;
      }
      auto ent = appsWithHotKeys[index].value();
      bool isX11 = registry && registry->all_of<X11App>(ent);
      currentlyFocusedApp = ent;
      WL_WM_LOG("WM: hotkey set current ent=%d\n",
                (int)entt::to_integral(ent));
      // Ensure renderer/input know the focus immediately; selection happens after the move.
      focusEntityAfterMove(ent);

      if (isX11) {
        auto& app = registry->get<X11App>(ent);
        app.deselect();
      }

      glm::vec3 targetPos{0.0f};
      glm::vec3 rotationDegrees{0.0f};
      if (!computeAppCameraTarget(ent,
                                  targetPos,
                                  rotationDegrees,
                                  "hotkey focus target")) {
        WL_WM_LOG("WM: hotkey focus idx=%d ent=%d no target -> immediate focus\n",
                  index,
                  (int)entt::to_integral(ent));
        if (isX11) {
          auto& app = registry->get<X11App>(ent);
          app.select();
        }
        focusEntityAfterMove(ent);
        return;
      }

      WL_WM_LOG("WM: hotkey focus idx=%d ent=%d after target controls=%d\n",
                index,
                (int)entt::to_integral(ent),
                controls ? 1 : 0);

      auto finishFocus = [this, ent, isX11]() {
        WL_WM_LOG("WM: hotkey focus finishing ent=%d isX11=%d\n",
                  (int)entt::to_integral(ent),
                  isX11 ? 1 : 0);
        if (registry && registry->valid(ent) && isX11) {
          auto& app = registry->get<X11App>(ent);
          app.select();
        }
        focusEntityAfterMove(ent);
      };

      if (controls) {
        WL_WM_LOG("WM: hotkey focus idx=%d ent=%d move start (controls)\n",
                  index,
                  (int)entt::to_integral(ent));
        controls->moveTo(targetPos,
                         rotationDegrees,
                         0.5f,
                         finishFocus);
      } else {
        // Fallback when controls are unavailable: move via camera then focus.
        glm::quat rotationQuat = glm::quat(glm::radians(rotationDegrees));
        glm::vec3 facing = rotationQuat * glm::vec3(0, 0, -1);
        WL_WM_LOG("WM: hotkey focus idx=%d ent=%d move start (no controls)\n",
                  index,
                  (int)entt::to_integral(ent));
        auto isDone = camera ? camera->moveTo(targetPos, facing, 0.5f) : nullptr;
        if (!isDone) {
          finishFocus();
          return;
        }
        auto weakDone = std::weak_ptr<bool>(isDone);
        std::thread([finishFocus, weakDone]() {
          while (true) {
            auto shared = weakDone.lock();
            if (!shared) {
              return;
            }
            if (*shared) {
              break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
          }
          finishFocus();
        }).detach();
      }
    }
  };

  switch (sym) {
    case XKB_KEY_E:
    case XKB_KEY_e:
      unfocusApp();
      break;
    case XKB_KEY_q:
    case XKB_KEY_Q:
      if (focused.has_value()) {
        if (waylandMode) {
          if (auto* comp = registry->try_get<WaylandApp::Component>(*focused)) {
            WL_WM_LOG("WM: hotkey close wayland ent=%d\n",
                      (int)entt::to_integral(*focused));
            if (comp->app) {
              comp->app->close();
            }
          }
        } else {
          auto& app = registry->get<X11App>(*focused);
          app.close();
        }
      }
      unfocusApp();
      break;
    case XKB_KEY_0:
      unfocusApp();
      if (controls) {
        controls->moveTo(glm::vec3(3.0, 5.0, 16), std::nullopt, 4);
      }
      break;
    case XKB_KEY_1:
    case XKB_KEY_2:
    case XKB_KEY_3:
    case XKB_KEY_4:
    case XKB_KEY_5:
    case XKB_KEY_6:
    case XKB_KEY_7:
    case XKB_KEY_8:
    case XKB_KEY_9: {
      int idx = static_cast<int>(sym - XKB_KEY_1);
      // Mirror hotkey presses into the compositor log so tests can verify cycling.
      if (FILE* f = std::fopen("/tmp/matrix-wlroots-output.log", "a")) {
        std::fprintf(f, "hotkey: idx=%d\n", idx);
        std::fclose(f);
      }
      WL_WM_LOG("WM: unfocusApp (hotkey) idx=%d\n", idx);
      swapOrFocus(idx);
      break;
    }
    default:
      break;
  }
}

void WindowManager::onHotkeyPress(XKeyEvent event) {
  pruneInvalidFocus();
  KeyCode eKeyCode = XKeysymToKeycode(display, XK_e);
  KeyCode qKeyCode = XKeysymToKeycode(display, XK_q);
  KeyCode oneKeyCode = XKeysymToKeycode(display, XK_1);

  KeyCode windowLargerCode = XKeysymToKeycode(display, XK_equal);
  KeyCode windowSmallerCode = XKeysymToKeycode(display, XK_minus);

  if (event.keycode == eKeyCode && event.state & hotkeyModifierMask) {
    // Hotkey modifier + E is pressed
    unfocusApp();
  }
  if (event.keycode == qKeyCode && event.state & hotkeyModifierMask) {
    // Hotkey modifier + Q is pressed
    if (currentlyFocusedApp.has_value()) {
      auto& app = registry->get<X11App>(currentlyFocusedApp.value());
      app.close();
    }
  }

  if (event.keycode == windowLargerCode && event.state & hotkeyModifierMask) {
    if (currentlyFocusedApp.has_value()) {
      lock_guard<mutex> lock(renderLoopMutex);
      events.push_back(WindowEvent{ LARGER, currentlyFocusedApp.value() });
    }
  }

  if (event.keycode == windowSmallerCode && event.state & hotkeyModifierMask) {
    if (currentlyFocusedApp.has_value()) {
      lock_guard<mutex> lock(renderLoopMutex);
      events.push_back(WindowEvent{ SMALLER, currentlyFocusedApp.value() });
    }
  }

  for (int i = 0; i < min((int)appsWithHotKeys.size(), 9); i++) {
    KeyCode code = XKeysymToKeycode(display, XK_1 + i);
    if (event.keycode == code && event.state & hotkeyModifierMask && event.state & ShiftMask) {
      if (currentlyFocusedApp.has_value()) {
        int source = findAppsHotKey(currentlyFocusedApp.value());
        swapHotKeys(source, i);
        return;
      }
    }
    if (event.keycode == code && event.state & hotkeyModifierMask) {
      unfocusApp();
      if(appsWithHotKeys[i]) {
        controls->goToApp(appsWithHotKeys[i].value());
      }
    }
  }
  KeyCode code = XKeysymToKeycode(display, XK_0);
  if (event.keycode == code && event.state & hotkeyModifierMask) {
    unfocusApp();
    controls->moveTo(glm::vec3(3.0, 5.0, 16), nullopt, 4);
  }
}

void WindowManager::handleSubstructure() {
  if (waylandMode) {
    return;
  }
  for (;;) {
    {
      lock_guard<std::mutex> continueLock(continueMutex);
      if(!continueRunning) {
        break;
      }
    }
    XEvent e;
    XNextEvent(display, &e);
    stringstream eventInfo;
    Window root = XRootWindow(display, 0);

    XWindowAttributes attrs;

    switch (e.type) {
    case CreateNotify:
      logger->info("CreateNotify event");
      logger->flush();
      XGetWindowAttributes(display, e.xcreatewindow.window, &attrs);
      if (e.xcreatewindow.override_redirect == True) {
        if(e.xcreatewindow.width > 30) {
          createApp(e.xcreatewindow.window, e.xcreatewindow.width,
                    e.xcreatewindow.height);
        }
      }
      break;
    case DestroyNotify:
      logger->info("DestroyNotify event");
      logger->flush();
      removeAppForWindow(e.xdestroywindow.window);
      break;
    case UnmapNotify:
      logger->info("UnmapNotify event");
      removeAppForWindow(e.xunmap.window);
      break;

    case MapNotify:
      {
        logger->info("MapNotify event");
        renderLoopMutex.lock();
        bool alreadyCreated = dynamicApps.contains(e.xmap.window);
        renderLoopMutex.unlock();
        XGetWindowAttributes(display, e.xmap.window, &attrs);
        if (!alreadyCreated && e.xmap.override_redirect == True) {
          if(attrs.width > 30) {
            createApp(e.xmap.window, attrs.width,
                attrs.height);
          }
        }
        break;
      }
    case MapRequest:
      logger->info("MapRequest event");
      onMapRequest(e.xmaprequest);
      break;
    case KeyPress:
      onHotkeyPress(e.xkey);
      break;
    case ConfigureNotify:
      logger->info("ConfigureNotify");
      reconfigureWindow(e.xconfigure);
      break;
    }
  }
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

void WindowManager::tick() {
  // In Wayland mode, there are no X11 events to process, but we still want
  // camera animations (moveTo) to advance each frame.
  if (waylandMode) {
    if (camera) {
      camera->tick();
      int focusedEnt = currentlyFocusedApp.has_value()
                         ? (int)entt::to_integral(*currentlyFocusedApp)
                         : -1;
      WL_WM_LOG("WM: tick (wayland) camPos=(%.2f,%.2f,%.2f) focused=%d\n",
                camera->position.x,
                camera->position.y,
                camera->position.z,
                focusedEnt);
    }
    return;
  }
  lock_guard<mutex> lock(renderLoopMutex);

  for (auto it = events.begin(); it != events.end(); it++) {
    auto app = registry->try_get<X11App>(it->window);
    if(it->type == SMALLER) {
      app->smaller();
    }
    if(it->type == LARGER) {
      app->larger();
    }
  }
  events.clear();

  for (auto it = appsToRemove.begin(); it != appsToRemove.end(); it++) {
    try {
      if (currentlyFocusedApp == *it) {
        unfocusApp();
      }
      if (registry->all_of<X11App>(*it)) {
        space->removeApp(*it);
        if (registry->orphan(*it)) {
          registry->destroy(*it);
        }
      }
    } catch (exception &e) {
      logger->error(e.what());
      logger->flush();
    }
  }
  appsToRemove.clear();

  vector<X11App *> waitForRemoval;
  for (auto it = appsToAdd.begin(); it != appsToAdd.end(); it++) {
    try {

      auto appEntity = dynamicApps[(*it)->getWindow()];

      if(registry->valid(appEntity)) {
        if(registry->all_of<X11App>(appEntity)) {
          waitForRemoval.push_back(*it);
        } else {
          registry->emplace<X11App>(appEntity, std::move(**it));
          delete *it;

          auto spawnAtCamera = !currentlyFocusedApp.has_value();
          space->addApp(appEntity, spawnAtCamera);

          if(registry->all_of<X11App>(appEntity)) {
            createUnfocusHackThread(appEntity);
          } else {
            dynamicApps.erase((*it)->getWindow());
          }
        }
      }
    } catch (exception &e) {
      logger->error(e.what());
      logger->flush();
    }
  }
  adjustAppsToAddAfterAdditions(waitForRemoval);
}

void WindowManager::focusApp(entt::entity appEntity) {
  if (waylandMode) {
    // Wayland focus is handled via wlroots; record focus only.
    currentlyFocusedApp = appEntity;
    if (registry && registry->valid(appEntity) &&
        registry->all_of<WaylandApp::Component>(appEntity)) {
      // Notify the Wayland client so keyboard/pointer focus matches click focus.
      if (auto* comp = registry->try_get<WaylandApp::Component>(appEntity)) {
        if (comp->app) {
          comp->app->takeInputFocus();
        }
      }
    }
    // Also move camera toward the app for waylandMode when focused via API.
    goToLookedAtApp();
    return;
  }
  currentlyFocusedApp = appEntity;
  auto &app = registry->get<X11App>(appEntity);
  app.focus(matrix);
}

void WindowManager::unfocusApp() {
  pruneInvalidFocus();
  if (waylandMode) {
    auto ent = currentlyFocusedApp.has_value() ? (int)entt::to_integral(*currentlyFocusedApp)
                                               : -1;
    size_t wlCount = 0;
    if (registry) {
      auto view = registry->view<WaylandApp::Component>();
      view.each([&](auto /*e*/, auto& /*comp*/) { ++wlCount; });
    }
    WL_WM_LOG("WM: unfocusApp (wayland) ent=%d wlCount=%zu\n", ent, wlCount);
    currentlyFocusedApp = std::nullopt;
    return;
  }
  logger->debug("unfocusing app");
  if (!currentlyFocusedApp.has_value()) {
    return;
  }
  auto ent = currentlyFocusedApp.value();
  if (!registry || !registry->valid(ent)) {
    currentlyFocusedApp = std::nullopt;
    return;
  }
  auto& app = registry->get<X11App>(ent);
  app.unfocus(matrix);
  currentlyFocusedApp = std::nullopt;
  WL_WM_LOG("WM: unfocusApp (x11) ent=%d\n", (int)entt::to_integral(ent));
}

void WindowManager::pruneInvalidFocus()
{
  if (!registry) {
    currentlyFocusedApp = std::nullopt;
    return;
  }
  if (currentlyFocusedApp && !registry->valid(*currentlyFocusedApp)) {
    currentlyFocusedApp = std::nullopt;
  }
  for (auto& opt : appsWithHotKeys) {
    if (opt && !registry->valid(*opt)) {
      opt = std::nullopt;
    }
  }
}

void WindowManager::focusLookedAtApp() {
  if (!space) {
    return;
  }
  if (auto looked = space->getLookedAtApp()) {
    auto entity = looked.value();
    WL_WM_LOG("WM: focusLookedAtApp entity=%d isWayland=%d isX11=%d pos=(%.2f,%.2f,%.2f)\n",
              (int)entt::to_integral(entity),
              registry->all_of<WaylandApp::Component>(entity) ? 1 : 0,
              registry->all_of<X11App>(entity) ? 1 : 0,
              registry->all_of<Positionable>(entity) ? registry->get<Positionable>(entity).pos.x : 0.0f,
              registry->all_of<Positionable>(entity) ? registry->get<Positionable>(entity).pos.y : 0.0f,
              registry->all_of<Positionable>(entity) ? registry->get<Positionable>(entity).pos.z : 0.0f);
    if (registry->all_of<WaylandApp::Component>(entity)) {
      // Wayland app path: just record focus and let wlroots handle input.
      currentlyFocusedApp = entity;
      if (auto* comp = registry->try_get<WaylandApp::Component>(entity)) {
        if (comp->app) {
          comp->app->takeInputFocus();
        }
      }
    } else if (registry->all_of<X11App>(entity)) {
      focusApp(entity);
    }
    goToLookedAtApp();
  }
}

void
WindowManager::setCursorVisible(bool visible)
{
  cursorVisible = visible;
}

void
WindowManager::keyReplay(const std::vector<std::pair<std::string, uint32_t>>& entries)
{
  replayQueue.clear();
  replayIndex = 0;
  replayActive = false;
  replayStart = std::chrono::steady_clock::now();
  uint64_t cumulative = 0;
  for (const auto& e : entries) {
    if (e.first.empty()) {
      continue;
    }
    xkb_keysym_t sym =
      xkb_keysym_from_name(e.first.c_str(), XKB_KEYSYM_NO_FLAGS);
    if (sym == XKB_KEY_NoSymbol) {
      continue;
    }
    cumulative += e.second;
    char name[64] = {0};
    xkb_keysym_get_name(sym, name, sizeof(name));
    // Detailed replay log to debug dropped/garbled input; keep to avoid
    // reintroducing silent failures in future refactors.
    WL_WM_LOG("WM: keyReplay add sym=%s(%u) delay=%u cumulative=%llu\n",
              name,
              sym,
              e.second,
              (unsigned long long)cumulative);
    replayQueue.push_back(ReplayEvent{ sym, cumulative });
  }
  WL_WM_LOG("WM: keyReplay queued count=%zu cumulative=%llu\n",
            replayQueue.size(),
            (unsigned long long)cumulative);
  replayActive = !replayQueue.empty();
}

std::vector<ReplayEvent>
WindowManager::consumeReadyReplaySyms(uint64_t now_ms)
{
  std::vector<ReplayEvent> ready;
  if (!replayActive) {
    return ready;
  }
  uint64_t start_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        replayStart.time_since_epoch())
                        .count();
  uint64_t elapsed = now_ms > start_ms ? now_ms - start_ms : 0;
  while (replayIndex < replayQueue.size() &&
         elapsed >= replayQueue[replayIndex].ready_ms) {
    ready.push_back(replayQueue[replayIndex]);
    ++replayIndex;
  }
  if (replayIndex >= replayQueue.size()) {
    replayActive = false;
  }
  return ready;
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

void WindowManager::setWMProps(Window root) {

  Window screen_owner = XCreateSimpleWindow(display, root, 0, 0, 1, 1, 0, 0, 0);
  Xutf8SetWMProperties(display, screen_owner, "matrixWM", "matrixWM", NULL,
                       0, NULL, NULL, NULL);

  char name[] = "_NET_WM_CM_S##";
  snprintf(name, sizeof(name), "_NET_WM_CM_S%d", screen);

  Atom atom = XInternAtom(display, name, 0);
  XSetSelectionOwner(display, atom, screen_owner, 0);
  // Set the _NET_SUPPORTED property
  Atom net_supported = XInternAtom(display, "_NET_SUPPORTED", False);
  Atom net_supported_atoms[] = {
      XInternAtom(display, "_NET_WM_ACTION_CLOSE", False),
      XInternAtom(display, "_NET_WM_ACTION_MOVE", False),
      XInternAtom(display, "_NET_WM_ACTION_RESIZE", False),
      XInternAtom(display, "_NET_WM_ACTION_MINIMIZE", False),
      XInternAtom(display, "_NET_WM_ACTION_FULLSCREEN", False),
      XInternAtom(display, "_NET_WM_ACTION_SHADE", False),
      XInternAtom(display, "_NET_WM_OPACITY", False),
      XInternAtom(display, "_NET_WM_STATE_MODAL", False),
      XInternAtom(display, "_NET_WM_STATE_STICKY", False),
      XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False),
      XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False),
      XInternAtom(display, "_NET_WM_STATE_SHADED", False),
      XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False),
      XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False),
      XInternAtom(display, "_NET_WM_STATE_HIDDEN", False),
      XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False),
      XInternAtom(display, "_NET_WM_STATE_ABOVE", False),
      XInternAtom(display, "_NET_WM_STATE_BELOW", False),
      XInternAtom(display, "_NET_WM_STATE_DEMANDS_ATTENTION", False)
  };

  int num_supported_atoms = sizeof(net_supported_atoms) / sizeof(Atom);

  // Set _NET_SUPPORTED property on the root window
  XChangeProperty(display, root, XInternAtom(display, "_NET_SUPPORTED", False),
                  XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)net_supported_atoms, num_supported_atoms);

  int workarea_x = 0;
  int workarea_y = 0;
  int workarea_width = SCREEN_WIDTH;
  int workarea_height = SCREEN_HEIGHT;

  // Convert the work area rectangle into a format suitable for the
  // _NET_WORKAREA property
  long workarea[] = {workarea_x, workarea_y, workarea_width, workarea_height};

  // Set the _NET_WORKAREA property on the root window
  Atom net_workarea = XInternAtom(display, "_NET_WORKAREA", False);
  XChangeProperty(display, root, net_workarea, XA_CARDINAL, 32, PropModeReplace,
                  (unsigned char *)&workarea, 4);

  long desktop_viewport[] = {0, 0};

  // Set the _NET_DESKTOP_VIEWPORT property on the root window
  Atom net_desktop_viewport =
      XInternAtom(display, "_NET_DESKTOP_VIEWPORT", False);
  XChangeProperty(display, root, net_desktop_viewport, XA_CARDINAL, 32,
                  PropModeReplace, (unsigned char *)&desktop_viewport, 2);

  // Set the _NET_SUPPORTING_WM_CHECK property on the root window
  Atom net_supporting_wm_check =
      XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
  XChangeProperty(display, root, net_supporting_wm_check,
                  XA_WINDOW, 32, PropModeReplace,
                  (unsigned char *)&screen_owner, 1);

  // Set the _NET_SUPPORTING_WM_CHECK property on matrix
  XChangeProperty(display, screen_owner, net_supporting_wm_check,
                  XA_WINDOW, 32, PropModeReplace, (unsigned char *)&screen_owner, 1);

  // Set the _NET_WM_NAME property of the matrix window
  Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
  XChangeProperty(display, screen_owner, net_wm_name, XA_STRING, 8,
                  PropModeReplace, (unsigned char *)"matrixWM", strlen("matrixWM"));

  XFlush(display);
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
                             Window matrix,
                             spdlog::sink_ptr loggerSink,
                             char** envp)
    : matrix(matrix), logSink(loggerSink), registry(registry), envp(envp) {

  menuProgram = Config::singleton()->get<std::string>("menu_program");
  if (const char* envMenu = std::getenv("MENU_PROGRAM")) {
    menuProgram = envMenu;
  }
  hotkeyModifierMask = resolveHotkeyMaskFromConfig();
  setupLogger();
  display = XOpenDisplay(NULL);
  screen = XDefaultScreen(display);
  X11App::initAppClass(display, screen);
  Window root = RootWindow(display, screen);
  setWMProps(root);
  XCompositeRedirectSubwindows(display, RootWindow(display, screen),
                                 CompositeRedirectAutomatic);

  overlay = XCompositeGetOverlayWindow(display, root);
  XReparentWindow(display, matrix, overlay, 0, 0);

  XFixesSelectCursorInput(display, overlay, XFixesDisplayCursorNotifyMask);

  XSelectInput(display, root,
               EnterWindowMask | SubstructureRedirectMask |
                   SubstructureNotifyMask | FocusChangeMask | LeaveWindowMask |
                   EnterWindowMask);

  XSelectInput(display, matrix, FocusChangeMask | LeaveWindowMask);

  for (int i = 0; i < 10; i++) {
    KeyCode code = XKeysymToKeycode(display, XK_0 + i);
    XGrabKey(display, code, hotkeyModifierMask, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, code, hotkeyModifierMask | ShiftMask, root, true, GrabModeAsync, GrabModeAsync);
  }
  XSync(display, false);
  XFlush(display);

  passthroughInput();
  allow_input_passthrough(overlay);
  substructureThread = thread(&WindowManager::handleSubstructure, this);
  substructureThread.detach();
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
                                               int screenY) {
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
    entity, app, accessory, layerShell, parent, offsetX, offsetY, screenX, screenY);
  // Attach textures immediately so layer shells/popups can be blitted directly.
  if (renderer) {
    renderer->registerApp(app.get());
  } else {
    WL_WM_LOG("WM: renderer missing; registered component only for entity=%d\n",
              (int)entt::to_integral(entity));
  }
  // Accessory apps (e.g. popups/menus) should not be positionable or bound to
  // hotkeys; they are rendered relative to their parent.
  if (accessory) {
    return entity;
  }

  // Place in world space similar to spawnAtCamera path.
  glm::vec3 pos(0.0f, 3.5f, -2.0f);
  glm::vec3 rot(0.0f);
  if (spawnAtCamera && camera) {
    float yaw = camera->getYaw();
    float pitch = camera->getPitch();
    glm::quat yawRotation =
      glm::angleAxis(glm::radians(90 + yaw), glm::vec3(0.0f, -1.0f, 0.0f));
    glm::quat pitchRotation =
      glm::angleAxis(glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
    glm::quat finalRotation = yawRotation * pitchRotation;
    rot = glm::degrees(glm::eulerAngles(finalRotation));
    pos = camera->position + finalRotation * glm::vec3(0, 0, -2.0f);
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
  appsWithHotKeys.push_back(entity);
  return entity;
}

} // namespace WindowManager
#ifndef WLROOTS_DEBUG_LOGS
#define WLROOTS_DEBUG_LOGS
#endif
