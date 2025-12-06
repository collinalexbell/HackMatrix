#include "screen.h"
#include <X11/Xlib.h>
#include <iostream>

float
getScreenHeight()
{
  Display* display = XOpenDisplay(NULL);
  if (display == NULL) {
    std::cout << "Failed to open display, defaulting height to 720"
              << std::endl;
    return 720.0f;
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
    std::cout << "Failed to open display, defaulting width to 1280"
              << std::endl;
    return 1280.0f;
  }

  int screen = DefaultScreen(display);
  float width = DisplayWidth(display, screen);
  XCloseDisplay(display);
  return width;
}

float SCREEN_WIDTH = getScreenWidth();
float SCREEN_HEIGHT = getScreenHeight();
