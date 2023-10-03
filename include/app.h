#ifndef __APP_H__
#define __APP_H__

#include <string>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/XKBlib.h>
#include <glad/glad_glx.h>

using namespace std;

class X11App {
  Display* display;
  int screen;
  Window appWindow;
  XWindowAttributes attrs;
  GLXFBConfig* fbConfigs;
  int fbConfigCount;
  void fetchInfo(string windowName);
 public:
  X11App(string windowName);
  void appTexture();
  void focus(Window matrix);
  void unfocus(Window matrix);
  bool isFocused = false;
};

#endif
