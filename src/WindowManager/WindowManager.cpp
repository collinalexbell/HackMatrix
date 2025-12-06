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
#include <cstddef>
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

#define OBS false
#define EDGE false
#define TERM false
// todo: Magica still doesn't work with dynamic loading
#define MAGICA false

namespace WindowManager {

void WindowManager::menu() {
  std::thread([this] { std::system(menuProgram.c_str()); }).detach();
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
WindowManager::wire(shared_ptr<WindowManager> sharedThis,
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

void WindowManager::goToLookedAtApp() {
  if (!space || !camera) {
    return;
  }
  auto looked = space->getLookedAtApp();
  if (!looked) {
    return;
  }
  auto ent = looked.value();
  glm::vec3 targetPos = camera->position;
  glm::vec3 facing = camera->front;
  if (registry->all_of<Positionable>(ent)) {
    // Match the X11 app navigation flow: stand back from the surface based on
    // its size and face the surface-normal derived from its rotation.
    float deltaZ = space->getViewDistanceForWindowSize(ent);
    glm::vec3 rotationDegrees = space->getAppRotation(ent);
    glm::quat rotationQuat = glm::quat(glm::radians(rotationDegrees));

    glm::vec3 appPos = space->getAppPosition(ent);
    targetPos = appPos + rotationQuat * glm::vec3(0, 0, deltaZ);
    facing = rotationQuat * glm::vec3(0, 0, -1);

    FILE* f = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
    if (f) {
      std::fprintf(f,
                   "WM: goToLookedAtApp ent=%d pos=(%.2f,%.2f,%.2f) camPos=(%.2f,%.2f,%.2f) targetPos=(%.2f,%.2f,%.2f)\n",
                   (int)entt::to_integral(ent),
                   appPos.x, appPos.y, appPos.z,
                   camera->position.x, camera->position.y, camera->position.z,
                   targetPos.x, targetPos.y, targetPos.z);
      std::fflush(f);
      std::fclose(f);
    }
  }
  camera->moveTo(targetPos, facing, 0.5f);
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

void WindowManager::onHotkeyPress(XKeyEvent event) {
  KeyCode eKeyCode = XKeysymToKeycode(display, XK_e);
  KeyCode qKeyCode = XKeysymToKeycode(display, XK_q);
  KeyCode oneKeyCode = XKeysymToKeycode(display, XK_1);

  KeyCode windowLargerCode = XKeysymToKeycode(display, XK_equal);
  KeyCode windowSmallerCode = XKeysymToKeycode(display, XK_minus);

  if (event.keycode == eKeyCode && event.state & Mod4Mask) {
    // Windows Key (Super_L) + Ctrl + E is pressed
    unfocusApp();
  }
  if (event.keycode == qKeyCode && event.state & Mod4Mask) {
    // Windows Key (Super_L) + Ctrl + E is pressed
    if (currentlyFocusedApp.has_value()) {
      auto& app = registry->get<X11App>(currentlyFocusedApp.value());
      app.close();
    }
  }

  if (event.keycode == windowLargerCode && event.state & Mod4Mask) {
    if (currentlyFocusedApp.has_value()) {
      lock_guard<mutex> lock(renderLoopMutex);
      events.push_back(WindowEvent{ LARGER, currentlyFocusedApp.value() });
    }
  }

  if (event.keycode == windowSmallerCode && event.state & Mod4Mask) {
    if (currentlyFocusedApp.has_value()) {
      lock_guard<mutex> lock(renderLoopMutex);
      events.push_back(WindowEvent{ SMALLER, currentlyFocusedApp.value() });
    }
  }

  for (int i = 0; i < min((int)appsWithHotKeys.size(), 9); i++) {
    KeyCode code = XKeysymToKeycode(display, XK_1 + i);
    if (event.keycode == code && event.state & Mod4Mask && event.state & ShiftMask) {
      if (currentlyFocusedApp.has_value()) {
        int source = findAppsHotKey(currentlyFocusedApp.value());
        swapHotKeys(source, i);
        return;
      }
    }
    if (event.keycode == code && event.state & Mod4Mask) {
      unfocusApp();
      if(appsWithHotKeys[i]) {
        controls->goToApp(appsWithHotKeys[i].value());
      }
    }
  }
  KeyCode code = XKeysymToKeycode(display, XK_0);
  if (event.keycode == code && event.state & Mod4Mask) {
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
      FILE* f = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
      if (f) {
        std::fprintf(f,
                     "WM: tick (wayland) camPos=(%.2f,%.2f,%.2f)\n",
                     camera->position.x,
                     camera->position.y,
                     camera->position.z);
        std::fflush(f);
        std::fclose(f);
      }
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
    // Also move camera toward the app for waylandMode when focused via API.
    goToLookedAtApp();
    return;
  }
  currentlyFocusedApp = appEntity;
  auto &app = registry->get<X11App>(appEntity);
  app.focus(matrix);
}

void WindowManager::unfocusApp() {
  if (waylandMode) {
    currentlyFocusedApp = std::nullopt;
    return;
  }
  logger->debug("unfocusing app");
  if (currentlyFocusedApp.has_value()) {
    auto &app = registry->get<X11App>(currentlyFocusedApp.value());
    app.unfocus(matrix);
    currentlyFocusedApp = std::nullopt;
  }
}

void WindowManager::focusLookedAtApp() {
  if (!space) {
    return;
  }
  if (auto looked = space->getLookedAtApp()) {
    auto entity = looked.value();
    FILE* f = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
    if (f) {
      std::fprintf(f,
                   "WM: focusLookedAtApp entity=%d isWayland=%d isX11=%d pos=(%.2f,%.2f,%.2f)\n",
                   (int)entt::to_integral(entity),
                   registry->all_of<WaylandApp::Component>(entity) ? 1 : 0,
                   registry->all_of<X11App>(entity) ? 1 : 0,
                   registry->all_of<Positionable>(entity) ? registry->get<Positionable>(entity).pos.x : 0.0f,
                   registry->all_of<Positionable>(entity) ? registry->get<Positionable>(entity).pos.y : 0.0f,
                   registry->all_of<Positionable>(entity) ? registry->get<Positionable>(entity).pos.z : 0.0f);
      std::fflush(f);
      std::fclose(f);
    }
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
  logger = make_shared<spdlog::logger>("wm", logSink);
  logger->set_level(spdlog::level::debug);
  logger->flush_on(spdlog::level::info);
  logger->debug("WindowManager()");
}

WindowManager::WindowManager(shared_ptr<EntityRegistry> registry, Window matrix,
                             spdlog::sink_ptr loggerSink)
    : matrix(matrix), logSink(loggerSink), registry(registry) {

  menuProgram = Config::singleton()->get<std::string>("menu_program");
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
    XGrabKey(display, code, Mod4Mask, root, true, GrabModeAsync, GrabModeAsync);
    XGrabKey(display, code, Mod4Mask | ShiftMask, root, true, GrabModeAsync, GrabModeAsync);
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
                             bool waylandMode)
  : logSink(loggerSink)
  , registry(registry)
  , waylandMode(waylandMode)
{
  menuProgram = Config::singleton()->get<std::string>("menu_program");
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

entt::entity WindowManager::registerWaylandApp(std::shared_ptr<WaylandApp> app, bool spawnAtCamera) {
  if (!app || !registry) {
    return entt::null;
  }
  static FILE* logFile = []() {
    FILE* f = std::fopen("/tmp/matrix-wlroots-wm.log", "a");
    return f ? f : stderr;
  }();
  std::fprintf(logFile,
               "WM: registerWaylandApp size=%dx%d spawnAtCamera=%d\n",
               app->getWidth(),
               app->getHeight(),
               spawnAtCamera ? 1 : 0);
  std::fflush(logFile);
  entt::entity entity = registry->create();
  registry->emplace<WaylandApp::Component>(entity, app);
  if (renderer) {
    renderer->registerApp(app.get());
  } else {
    std::fprintf(logFile,
                 "WM: renderer missing; registered component only for entity=%d\n",
                 (int)entt::to_integral(entity));
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
  std::fprintf(logFile,
               "WM: WaylandApp entity=%d size=%dx%d pos=(%.2f, %.2f, %.2f)\n",
               (int)entt::to_integral(entity),
               app->getWidth(),
               app->getHeight(),
               pos.x,
               pos.y,
               pos.z);
  std::fflush(logFile);
  return entity;
}

} // namespace WindowManager
