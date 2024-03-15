#pragma once
#include "WindowManager/Space.h"
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
  static constexpr int APP_WIDTH = 1920 * .85;
  static constexpr int APP_HEIGHT = 1920 * .85 * .54;

  Display *display = NULL;
  Controls *controls = NULL;
  spdlog::sink_ptr logSink;
  int screen;
  X11App *emacs = NULL;
  X11App *magicaVoxel = NULL;
  X11App *microsoftEdge = NULL;
  X11App *obs = NULL;
  X11App *terminator = NULL;
  X11App *currentlyFocusedApp = NULL;
  shared_ptr<WindowManager::Space> space;
  Window matrix;
  Window overlay;
  atomic_bool firstRenderComplete = false;
  map<Window, X11App*> dynamicApps;
  mutex renderLoopMutex;
  vector<X11App*> appsToAdd;
  vector<X11App*> appsToRemove;
  void forkOrFindApp(string cmd, string pidOf, string className, X11App *&app,
                     char **envp, string args = "");
  std::thread substructureThread;
  void onDestroyNotify(XDestroyWindowEvent);
  void onMapRequest(XMapRequestEvent);
  std::shared_ptr<spdlog::logger> logger;
  void onHotkeyPress(XKeyEvent event);
  void unfocusApp();
  X11App* createApp(Window window, unsigned int width = APP_WIDTH, unsigned int height = APP_HEIGHT);
  void allow_input_passthrough(Window window);
  void capture_input(Window window, bool shapeBounding, bool shapeInput);
  void addApps();

public:
  void passthroughInput();
  void captureInput();
  void createAndRegisterApps(char **envp);
  WM(Window, spdlog::sink_ptr);
  ~WM();
  void focusApp(X11App *app);
  void wire(Camera* camera, Renderer* renderer);
  void handleSubstructure();
  void goToLookedAtApp();
  void registerControls(Controls *controls);
  void tick();
};
