#include "screen.h"
#include <X11/Xlib.h>
#include <iostream>

float
getScreenHeight()
{
  Display* display = XOpenDisplay(NULL);
  if (display == NULL) {
    std::cout << "Failed to open display" << std::endl;
    throw "couldn't determine screen height";
  }

  int screen = DefaultScreen(display);
  float height = DisplayHeight(display, screen);

  XCloseDisplay(display);
  return height;
}

float
getScreenWidth()
{
  Display* display = XOpenDisplay(NULL);
  if (display == NULL) {
    std::cout << "Failed to open display" << std::endl;
    throw "couldn't determine screen width";
  }

  int screen = DefaultScreen(display);
  float width = DisplayWidth(display, screen);
  XCloseDisplay(display);
  return width;
}

float SCREEN_WIDTH = getScreenWidth();
float SCREEN_HEIGHT = getScreenHeight();
