#pragma once
#include "app.h"
#include "world.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>

class WM {
  // hardcoded apps on boot (will fix later)
  Display *display;
  int screen;
  X11App *emacs;
  X11App *microsoftEdge;
  X11App *obs;
  X11App *terminator;
  World *world;
  void forkOrFindApp(string cmd, string pidOf, string className, X11App *&app,
                     char **envp);
  void allow_input_passthrough(Window window);

public:
  void createAndRegisterApps(char **envp);
  WM(Window overlay, Window matrix, Display *display, int screen);
  void attachWorld(World *world) {this->world = world;}
  void addAppsToWorld();
};
