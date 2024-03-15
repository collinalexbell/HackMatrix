#include "WindowManager/WindowManager.h"
#include "app.h"
#include "controls.h"
#include "entity.h"
#include "renderer.h"
#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <cstddef>
#include <glm/glm.hpp>
#include <iostream>

#include <X11/extensions/shape.h>
#include <memory>
#include <optional>
#include <spdlog/common.h>
#include <sstream>
#include <thread>
#include <unistd.h>

#define OBS false
#define EDGE true
#define TERM true
#define MAGICA true

namespace WindowManager {

void WindowManager::forkOrFindApp(string cmd, string pidOf, string className,
                                  entt::entity &appEntity, char **envp, string args) {
  char *line;
  std::size_t len = 0;
  FILE *pidPipe = popen(string("pgrep " + pidOf).c_str(), "r");
  if (getline(&line, &len, pidPipe) == -1) {
    int pid = fork();
    if (pid == 0) {
      setsid();
      if (args != "") {
        execle(cmd.c_str(), cmd.c_str(), args.c_str(), NULL, envp);
      }
      execle(cmd.c_str(), cmd.c_str(), NULL, envp);
      exit(0);
    }
    if (className == "obs") {
      sleep(30);
    } else if (className == "magicavoxel.exe") {
      sleep(6);
    } else {
      sleep(4);
    }
  }
  X11App* app = X11App::byClass(className, display, screen, APP_WIDTH, APP_HEIGHT);
  appEntity = registry->create();
  registry->emplace<X11App>(appEntity, std::move(*app));
  dynamicApps[app->getWindow()] = appEntity;
  logger->info("created " + className + " app");
}

void WindowManager::createAndRegisterApps(char **envp) {
  logger->info("enter createAndRegisterApps()");

  forkOrFindApp("/usr/bin/emacs", "emacs", "Emacs", emacs, envp);
  if (MAGICA) {
    forkOrFindApp("/usr/bin/wine", "MagicaVoxel.exe", "magicavoxel.exe",
                  magicaVoxel, envp,
                  "/home/collin/magicavoxel/MagicaVoxel.exe");
  }
  if (EDGE) {
    forkOrFindApp("/usr/bin/microsoft-edge", "msedge", "Microsoft-edge",
                  microsoftEdge, envp);
  }
  if (TERM) {
    forkOrFindApp("/usr/bin/terminator", "terminator", "Terminator", terminator,
                  envp);
  }
  if (OBS) {
    forkOrFindApp("/usr/bin/obs", "obs", "obs", obs, envp);
  }

  logger->info("exit createAndRegisterApps()");
}

void WindowManager::allow_input_passthrough(Window window) {
  XserverRegion region = XFixesCreateRegion(display, NULL, 0);

  XFixesSetWindowShapeRegion(display, window, ShapeBounding, 0, 0, 0);
  XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, region);

  XFixesDestroyRegion(display, region);
}

void WindowManager::passthroughInput() {
  allow_input_passthrough(overlay);
  allow_input_passthrough(matrix);
  XFlush(display);
}

void WindowManager::capture_input(Window window, bool shapeBounding,
                                  bool shapeInput) {
  // Create a region covering the entire window
  XserverRegion region = XFixesCreateRegion(display, NULL, 0);
  XRectangle rect;
  rect.x = 0;
  rect.y = 0;
  rect.width = 1920;  // Replace with your window's width
  rect.height = 1080; // Replace with your window's height
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
  capture_input(overlay, true, true);
  capture_input(matrix, true, true);
  XFlush(display);
}

void WindowManager::addApps() {
  space->addApp(emacs);
  if (MAGICA) {
    space->addApp(magicaVoxel);
  }
  if (TERM) {
    space->addApp(terminator);
  }
  if (EDGE) {
    space->addApp(microsoftEdge);
  }
  if (OBS) {
    space->addApp(obs);
  }
}

void WindowManager::wire(Camera *camera, Renderer *renderer) {
  space = make_shared<Space>(registry, renderer, camera, logSink);
  renderer->wireWindowManagerSpace(space);
  controls->wireWindowManager(space);
  addApps();
}

void WindowManager::createApp(Window window, unsigned int width,
                                 unsigned int height) {
  auto entity = registry->create();
  X11App *app = X11App::byWindow(window, display, screen, width, height);
  renderLoopMutex.lock();
  appsToAdd.push_back(app);
  dynamicApps[window] = entity;
  renderLoopMutex.unlock();
}

void WindowManager::onMapRequest(XMapRequestEvent event) {
  char *name;
  XFetchName(display, event.window, &name);
  string sName(name);
  bool alreadyRegistered = dynamicApps.count(event.window);
  if (!alreadyRegistered && !sName.ends_with("one")) {
    stringstream ss;
    ss << "window created: " << event.window << " " << name;
    logger->info(ss.str());
    logger->flush();
    createApp(event.window);
  }
}

void WindowManager::onDestroyNotify(XDestroyWindowEvent event) {
  renderLoopMutex.lock();
  if (dynamicApps.contains(event.window)) {
    auto appEntity = dynamicApps.at(event.window);
    dynamicApps.erase(event.window);
    appsToRemove.push_back(appEntity);
  }
  renderLoopMutex.unlock();
}

void WindowManager::onHotkeyPress(XKeyEvent event) {
  KeyCode eKeyCode = XKeysymToKeycode(display, XK_e);
  KeyCode oneKeyCode = XKeysymToKeycode(display, XK_1);
  vector<entt::entity> appsWithHotKeys = {emacs};
  if (EDGE) {
    appsWithHotKeys.push_back(microsoftEdge);
  }
  if (TERM) {
    appsWithHotKeys.push_back(terminator);
  }
  if (event.keycode == eKeyCode && event.state & Mod4Mask) {
    // Windows Key (Super_L) + Ctrl + E is pressed
    unfocusApp();
  }
  for (int i = 0; i < appsWithHotKeys.size(); i++) {
    KeyCode code = XKeysymToKeycode(display, XK_1 + i);
    if (event.keycode == code && event.state & Mod4Mask) {
      unfocusApp();
      controls->goToApp(appsWithHotKeys[i]);
    }
  }
  KeyCode code = XKeysymToKeycode(display, XK_0);
  if (event.keycode == code && event.state & Mod4Mask) {
    unfocusApp();
    controls->moveTo(glm::vec3(3.0, 5.0, 16), 4);
  }
}

void WindowManager::handleSubstructure() {
  for (;;) {
    XEvent e;
    XNextEvent(display, &e);
    stringstream eventInfo;
    Window root = XRootWindow(display, 0);

    XWindowAttributes attrs;

    switch (e.type) {
    case CreateNotify:
      logger->info("CreateNotify event");
      XGetWindowAttributes(display, e.xcreatewindow.window, &attrs);
      if (e.xcreatewindow.override_redirect == True) {
        createApp(e.xcreatewindow.window, e.xcreatewindow.width,
                             e.xcreatewindow.height);
        stringstream ss;
        ss << "CreateNotify event: position: ";
        ss << attrs.x << ",";
        ss << attrs.y;
        logger->debug(ss.str());
      }
      logger->flush();
      break;
    case DestroyNotify:
      logger->info("DestroyNotify event");
      logger->flush();
      onDestroyNotify(e.xdestroywindow);
      break;
    case MapRequest:
      logger->info("MapRequest event");
      onMapRequest(e.xmaprequest);
      break;
    case KeyPress:
      onHotkeyPress(e.xkey);
      break;
    }
  }
}

void WindowManager::tick() {
  renderLoopMutex.lock();
  for (auto it = appsToAdd.begin(); it != appsToAdd.end(); it++) {
    try {
      auto appEntity = dynamicApps[(*it)->getWindow()];
      registry->emplace<X11App>(appEntity, std::move(**it));
      delete *it;
      auto spawnAtCamera = !currentlyFocusedApp.has_value();
      space->addApp(appEntity, spawnAtCamera);
      if(registry->valid(appEntity)) {
        auto &app = registry->get<X11App>(appEntity);
        if (!app.isAccessory() && !currentlyFocusedApp.has_value()) {
          auto t = thread([this, appEntity]() -> void {
            auto app = registry->try_get<X11App>(appEntity);
            usleep(0.5 * 1000000);
            if(app != NULL) {
              app->unfocus(matrix);
            }
          });
          t.detach();
        }
      }
    } catch (exception &e) {
      logger->error(e.what());
      logger->flush();
    }
  }
  appsToAdd.clear();

  for (auto it = appsToRemove.begin(); it != appsToRemove.end(); it++) {
    try {
      if (currentlyFocusedApp == *it) {
        unfocusApp();
      }
      if(registry->all_of<X11App>(*it)) {
        space->removeApp(*it);
      } else {
        // empty entity
        registry->destroy(*it);
      }
    } catch (exception &e) {
      logger->error(e.what());
      logger->flush();
    }
  }
  appsToRemove.clear();

  renderLoopMutex.unlock();
}

void WindowManager::focusApp(entt::entity appEntity) {
  currentlyFocusedApp = appEntity;
  auto &app = registry->get<X11App>(appEntity);
  app.focus(matrix);
}

void WindowManager::unfocusApp() {
  if (currentlyFocusedApp.has_value()) {
    auto &app = registry->get<X11App>(currentlyFocusedApp.value());
    app.unfocus(matrix);
    currentlyFocusedApp = std::nullopt;
  }
}

void WindowManager::registerControls(Controls *controls) {
  this->controls = controls;
}

  WindowManager::WindowManager(shared_ptr<EntityRegistry> registry, Window matrix,
                             spdlog::sink_ptr loggerSink)
    : matrix(matrix), logSink(loggerSink), registry(registry) {
  logger = make_shared<spdlog::logger>("wm", loggerSink);
  logger->set_level(spdlog::level::debug);
  logger->flush_on(spdlog::level::info);
  logger->debug("WindowManager()");
  display = XOpenDisplay(NULL);
  screen = XDefaultScreen(display);
  X11App::initAppClass(display, screen);
  Window root = RootWindow(display, screen);
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

  Atom XdndSelectionAtom = XInternAtom(display, "XdndSelection", False);
  XSetSelectionOwner(display, overlay, matrix, CurrentTime);

  for (int i = 0; i < 10; i++) {
    KeyCode code = XKeysymToKeycode(display, XK_0 + i);
    XGrabKey(display, code, Mod4Mask, root, true, GrabModeAsync, GrabModeAsync);
  }
  XSync(display, false);
  XFlush(display);

  passthroughInput();
  allow_input_passthrough(overlay);
  substructureThread = thread(&WindowManager::handleSubstructure, this);
  substructureThread.detach();
}

WindowManager::~WindowManager() {
  XCompositeReleaseOverlayWindow(display, RootWindow(display, screen));
}

} // namespace WindowManager
