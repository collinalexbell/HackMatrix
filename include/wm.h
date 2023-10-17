#pragma once
#include "app.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "world.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <memory>
#include <thread>

class WM {
  // hardcoded apps on boot (will fix later)

  Display *display = NULL;
  int screen;
  X11App *emacs = NULL;
  X11App *microsoftEdge = NULL;
  X11App *obs = NULL;
  X11App *terminator = NULL;
  World *world = NULL;
  void forkOrFindApp(string cmd, string pidOf, string className, X11App *&app,
                     char **envp);
  void allow_input_passthrough(Window window);
  std::thread substructureThread;
  void onCreateNotify(XCreateWindowEvent);
  std::shared_ptr<spdlog::logger> logger;
public:
  void createAndRegisterApps(char **envp);
  WM(Window);
  ~WM();
  void attachWorld(World *world) {this->world = world;}
  void addAppsToWorld();
  void handleSubstructure();
};
