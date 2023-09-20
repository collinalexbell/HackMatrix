#ifndef __APP_H__
#define __APP_H__

#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/XKBlib.h>
#include <glad/glad_glx.h>

class X11App {
  Display* display;
  int screen;
  Window emacs;
  XWindowAttributes attrs;
  GLXFBConfig* fbConfigs;
  int fbConfigCount;
  void fetchInfo();
 public:
  X11App();
  void appTexture();
  void focus(Window matrix);
  void unfocus(Window matrix);
};

#endif
