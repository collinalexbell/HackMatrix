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
  int textureUnit = -1;
  int textureId = -1;
 public:
   int width;
   int height;
   X11App(string windowName, Display *display, int screen, int width,
          int height);
   void appTexture();
   void attachTexture(int textureUnit, int textureId);
   void click(int x, int y);
   void focus(Window matrix);
   void takeInputFocus();
   void unfocus(Window matrix);
   void resize(int width, int height);
   bool isFocused = false;
};

#endif
