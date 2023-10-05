#include <iostream>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/XKBlib.h>
#include <string>
#include <functional>
#include <glad/glad_glx.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thread>
#include "app.h"

using namespace std;

#define XA_ATOM 4

const int pixmap_config[] = {
    GLX_BIND_TO_TEXTURE_RGBA_EXT, 1, GLX_BIND_TO_TEXTURE_TARGETS_EXT,
    GLX_TEXTURE_2D_BIT_EXT, GLX_RENDER_TYPE, GLX_RGBA_BIT, GLX_DRAWABLE_TYPE,
    GLX_PIXMAP_BIT, GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR, GLX_X_RENDERABLE, 1,
    // GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT, (GLint) GLX_DONT_CARE,

    //		GLX_SAMPLE_BUFFERS, 1,
    //		GLX_SAMPLES, 4,
    GLX_DOUBLEBUFFER, 1, GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8, GLX_BLUE_SIZE, 8,
    GLX_ALPHA_SIZE, 8, GLX_STENCIL_SIZE, 0, GLX_DEPTH_SIZE, 16, 0};

void traverseWindowTree(Display* display, Window win, std::function<void(Display*, Window)> func) {
  func(display, win);

  // handle children
  Window parent;
  Window root;
  Window childrenMem[20];
  Window *children = childrenMem;
  unsigned int nchildren_return;

  XQueryTree(display, win, &root, &parent, &children, &nchildren_return);

  for(int i = 0; i < nchildren_return; i++) {
    traverseWindowTree(display, children[i], func);
  }
}

Window getWindowByName(Display* display, string search) {
  Window root = XDefaultRootWindow(display);
  Window rv;
  bool found = false;

  traverseWindowTree(display, root, [&rv, &found, search](Display* display, Window window) {
    char nameMem[100];
    char* name;
    XFetchName(display, window, &name);
    if(name != NULL && string(name) == search) {
      found = true;
      rv = window;
    }
  });

  if(!found) {
    cout << "unable to find window: \"" << search << "\""<< endl;
    throw "unable to find window";
  }
  return rv;
}

void X11App::fetchInfo(string windowName) {
  display = XOpenDisplay(NULL);
  screen = XDefaultScreen(display);

  if (!gladLoadGLXLoader((GLADloadproc)glfwGetProcAddress, display, screen)) {
    std::cout << "Failed to initialize GLAD for GLX" << std::endl;
    return;
  }

  Window win = XDefaultRootWindow(display);

  appWindow = getWindowByName(display, windowName);
  XMapWindow(display,appWindow);
  XGetWindowAttributes(display, appWindow, &attrs);

  fbConfigs = glXChooseFBConfig(display, 0, pixmap_config, &fbConfigCount);
}

int errorHandler(Display *dpy, XErrorEvent *err)
{
  char buf[5000];
  XGetErrorText(dpy, err->error_code, buf, 5000);
  printf("error: %s\n", buf);

  return 0;
}

X11App::X11App(string windowName) {
  //TODO: create the app in virtual X
  XSetErrorHandler(errorHandler);
  fetchInfo(windowName);
  XCompositeRedirectSubwindows(display, RootWindow(display, screen), CompositeRedirectAutomatic);
};

void X11App::unfocus(Window matrix) {
  isFocused=false;
  XSelectInput(display, matrix, 0);
  XSetInputFocus(display, matrix, RevertToParent, CurrentTime);
  XSync(display, False);
  XFlush(display);
}

void X11App::focus(Window matrix) {
  isFocused = true;
  Window root = DefaultRootWindow(display);
  XSetInputFocus(display, appWindow, RevertToParent, CurrentTime);
  XSelectInput(display, appWindow, KeyPressMask);
  XSync(display, False);
  XFlush(display);

  // Create a thread to listen for key events
  std::thread keyListenerThread([this, root, matrix]() {
    try {
      XEvent event;
      KeyCode eKeyCode = XKeysymToKeycode(display, XK_e);

      while (true) {
        XNextEvent(display, &event);

        if (event.type == KeyPress) {
          XKeyEvent keyEvent = event.xkey;
          if (keyEvent.keycode == eKeyCode &&
              keyEvent.state & ControlMask &&
              keyEvent.state & Mod1Mask) {
            // Windows Key (Super_L) + Ctrl + E is pressed
            unfocus(matrix); // Call your unfocus function
            break;
          }
        }
      }
    } catch (const std::exception& ex) {
      std::cerr << "Exception caught in keyListener: " << ex.what() << std::endl;
    }
  });

  keyListenerThread.detach();
}



void X11App::appTexture() {
  Pixmap pixmap = XCompositeNameWindowPixmap(display, appWindow);

  const int pixmap_attribs[] = {
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
    None
  };

  GLXPixmap glxPixmap = glXCreatePixmap(display, fbConfigs[0], pixmap, pixmap_attribs);
  glXBindTexImageEXT(display, glxPixmap, GLX_FRONT_LEFT_EXT, NULL);
}
