#pragma once
#include "app.h"
#include "logger.h"
#include "world.h"
#include "controls.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <atomic>
#include <map>
#include <memory>
#include <spdlog/common.h>
#include <thread>

class Controls;

class WM {
  // hardcoded apps on boot (will fix later)

  Display *display = NULL;
  Controls *controls = NULL;
  int screen;
  X11App *emacs = NULL;
  X11App *microsoftEdge = NULL;
  X11App *obs = NULL;
  X11App *terminator = NULL;
  X11App *currentlyFocusedApp = NULL;
  World *world = NULL;
  Window matrix;
  Window overlay;
  atomic_bool firstRenderComplete = false;
  map<Window, X11App*> dynamicApps;
  mutex renderLoopMutex;
  vector<X11App*> appsToAdd;
  vector<X11App*> appsToRemove;
  void forkOrFindApp(string cmd, string pidOf, string className, X11App *&app,
                     char **envp);
  std::thread substructureThread;
  void onDestroyNotify(XDestroyWindowEvent);
  void onMapRequest(XMapRequestEvent);
  std::shared_ptr<spdlog::logger> logger;
  void onHotkeyPress(XKeyEvent event);
  void unfocusApp();
  void createApp(Window window);
  void allow_input_passthrough(Window window);
  void capture_input(Window window, bool shapeBounding, bool shapeInput);

public:
  void passthroughInput();
  void captureInput();
  void createAndRegisterApps(char **envp);
  WM(Window, spdlog::sink_ptr);
  ~WM();
  void focusApp(X11App *app);
  void attachWorld(World *world) {this->world = world;}
  void addAppsToWorld();
  void handleSubstructure();
  void goToLookedAtApp();
  void registerControls(Controls *controls);
  void mutateWorld();
};
