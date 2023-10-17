#include "wm.h"
#include "app.h"
#include <X11/Xlib.h>
#include <glm/glm.hpp>
#include <iostream>

#include <X11/extensions/shape.h>
#include <spdlog/common.h>
#include <sstream>
#include <thread>

int APP_WIDTH = 1920 * .85;
int APP_HEIGHT = 1920 * .85 * .54;
void WM::forkOrFindApp(string cmd, string pidOf, string className, X11App *&app, char **envp) {
  char *line;
  std::size_t len = 0;
  FILE *pidPipe = popen(string("pidof " + pidOf).c_str(), "r");
  if (getline(&line, &len, pidPipe) == -1) {
    int pid = fork();
    if (pid == 0) {
      setsid();
      execle(cmd.c_str(), cmd.c_str(), NULL, envp);
      exit(0);
    }
    if (className == "obs") {
      sleep(30);
    } else {
      sleep(1);
    }
  }
  app = X11App::byClass(className, display, screen, APP_WIDTH, APP_HEIGHT);
}

void WM::createAndRegisterApps(char **envp) {
  forkOrFindApp("/usr/bin/emacs", "emacs", "Emacs", emacs, envp);
  forkOrFindApp("/usr/bin/microsoft-edge", "microsoft-edge", "Microsoft-edge",
                microsoftEdge, envp);
  forkOrFindApp("/usr/bin/terminator", "terminator", "Terminator", terminator,
                envp);
  forkOrFindApp("/usr/bin/obs", "obs", "obs", obs, envp);
}


void WM::allow_input_passthrough(Window window) {
  XserverRegion region = XFixesCreateRegion(display, NULL, 0);

  XFixesSetWindowShapeRegion(display, window, ShapeBounding, 0, 0, 0);
  XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, region);

  XFixesDestroyRegion(display, region);
}

void WM::addAppsToWorld() {
  world->addApp(glm::vec3(2.8, 1.0, 5.0), terminator);
  world->addApp(glm::vec3(4.0, 1.0, 5.0), emacs);
  world->addApp(glm::vec3(4.0, 1.75, 5.0), microsoftEdge);
  world->addApp(glm::vec3(5.2, 1.0, 5.0), obs);
}

void WM::onCreateNotify(XCreateWindowEvent event) {
  char *name;
  XFetchName(display, event.window, &name);
  stringstream ss;
  ss << "window created: " << event.window << " "<< name;
  logger->info(ss.str());
}

void WM::onHotkeyPress(XKeyEvent event) {
  KeyCode eKeyCode = XKeysymToKeycode(display, XK_e);
  KeyCode oneKeyCode = XKeysymToKeycode(display, XK_1);
  vector<X11App*> appsWithHotKeys = {emacs, microsoftEdge, terminator, obs};
  
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
}

void WM::handleSubstructure() {
  for (;;) {
    XEvent e;
    XNextEvent(display, &e);

    switch (e.type) {
    case CreateNotify:
      cout << "created window" << endl;
      onCreateNotify(e.xcreatewindow);
      break;
    case DestroyNotify:
      cout << "destroyed window" << endl;
      // OnDestroyNotify(e.xdestroywindow);
      break;
    case ReparentNotify:
      cout << "reparented window" << endl;
      // OnReparentNotify(e.xreparent);
      break;
    case KeyPress:
      onHotkeyPress(e.xkey);
      break;
    }
  }
}

void WM::focusApp(X11App* app) {
  currentlyFocusedApp = app;
  app->focus(matrix);
}

void WM::unfocusApp() {
  if(currentlyFocusedApp != NULL) {
    currentlyFocusedApp->unfocus(matrix);
    currentlyFocusedApp = NULL;
  }
}

void WM::registerControls(Controls *controls) {
  this->controls = controls;
}

WM::WM(Window matrix): matrix(matrix) {
  logger = spdlog::basic_logger_mt("wm_logger", "logs/wm.txt");
  logger->set_level(spdlog::level::info);
  logger->flush_on(spdlog::level::info);
  logger->info("WM()");
  display = XOpenDisplay(NULL);
  screen = XDefaultScreen(display);
  Window root = RootWindow(display, screen);
  XCompositeRedirectSubwindows(display, RootWindow(display, screen),
                               CompositeRedirectAutomatic);

  Window overlay = XCompositeGetOverlayWindow(display, root);
  XReparentWindow(display, matrix, overlay, 0, 0);

  XFixesSelectCursorInput(display, overlay, XFixesDisplayCursorNotifyMask);

  XSelectInput(display, root,
               SubstructureRedirectMask | SubstructureNotifyMask);


  for(int i = 0; i<9; i++) {
    KeyCode code = XKeysymToKeycode(display, XK_1+i);
    XGrabKey(display, code, Mod4Mask, root, true, GrabModeAsync, GrabModeAsync);
  }
  XSync(display, false);
  XFlush(display);

  allow_input_passthrough(overlay);
  allow_input_passthrough(matrix);
  substructureThread = thread(&WM::handleSubstructure, this);
  substructureThread.detach();
}

WM::~WM() {
  XCompositeReleaseOverlayWindow(display, RootWindow(display, screen));
}
