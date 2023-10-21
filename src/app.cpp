#include <X11/X.h>
#include <X11/Xutil.h>
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

#include <X11/extensions/Xfixes.h>
#include <X11/extensions/xfixeswire.h>

using namespace std;

#define XA_ATOM 4

const int pixmap_config[] = {
    GLX_BIND_TO_TEXTURE_RGB_EXT, 1,
    GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
    //GLX_RENDER_TYPE, GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT | GLX_WINDOW_BIT,
    //GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
    //GLX_X_RENDERABLE, 1,
    // GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT, (GLint) GLX_DONT_CARE,

    //		GLX_SAMPLE_BUFFERS, 1,
    //		GLX_SAMPLES, 4,
    GLX_DOUBLEBUFFER, 1,
    GLX_BUFFER_SIZE, 24,
    GLX_RED_SIZE, 8,
    GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE, 8,
    GLX_ALPHA_SIZE, 0,
    //GLX_STENCIL_SIZE, 0,
    //GLX_DEPTH_SIZE, 16,
    0};

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
    if(name != NULL && string(name).find(search) != string::npos) {
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


Window getWindowByClass(Display *display, string search) {
  Window root = XDefaultRootWindow(display);
  Window rv;
  int largestWidth = 0;
  bool found = false;

  traverseWindowTree(
                     display, root, [&rv, &found, search, &largestWidth](Display *display, Window window) {
        char nameMem[100];
        XClassHint classHint;
        classHint.res_class = NULL;
        classHint.res_name = NULL;
        char* className = NULL;
        XGetClassHint(display, window, &classHint);
        className = classHint.res_class;
        if (className != NULL &&
            string(className).find(search) != string::npos) {
          XWindowAttributes attrs;
          XGetWindowAttributes(display, window, &attrs);
          if(attrs.width > largestWidth) {
            largestWidth = attrs.width;
            rv = window;
            found = true;
          }
        }
                     });

  if (!found) {
    cout << "unable to find window: \"" << search << "\"" << endl;
    throw "unable to find window";
  }
  return rv;
}

void X11App::fetchInfo(Identifier identifier) {

  if (!gladLoadGLXLoader((GLADloadproc)glfwGetProcAddress, display, screen)) {
    std::cout << "Failed to initialize GLAD for GLX" << std::endl;
    return;
  }

  Window win = XDefaultRootWindow(display);

  switch(identifier.type) {
  case NAME:
    appWindow = getWindowByName(display, identifier.strId);
    break;
  case CLASS:
    appWindow = getWindowByClass(display, identifier.strId);
    break;
  case WINDOW:
    appWindow = identifier.intId;
  }
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

X11App::X11App(Display* display, int screen): display(display), screen(screen) {
  XSetErrorHandler(errorHandler);
};


X11App* X11App::byName(string windowName, Display *display, int screen,
                      int width, int height) {
  X11App* rv = new X11App(display, screen);
  Identifier id;
  id.type = NAME;
  id.strId = windowName;
  rv->fetchInfo(id);
  rv->resize(width, height);
  return rv;
}
X11App* X11App::byClass(string windowClass, Display *display, int screen,
                       int width, int height) {
  X11App *rv = new X11App(display, screen);
  Identifier id;
  id.type = CLASS;
  id.strId = windowClass;
  rv->fetchInfo(id);
  rv->resize(width, height);
  return rv;
}
X11App* X11App::byWindow(Window window, Display *display, int screen, int width, int height) {
  X11App *rv = new X11App(display, screen);
  Identifier id;
  id.type = WINDOW;
  id.intId = window;
  rv->fetchInfo(id);
  rv->resize(width, height);
  return rv;
}

void X11App::click(int x, int y) {
  cout << "click" << x << ", " << y << endl;
  XEvent event;
  event.xbutton.type = ButtonPress;
  event.xbutton.display = display;
  event.xbutton.window = appWindow;
  event.xbutton.root = DefaultRootWindow(display);
  event.xbutton.subwindow = None;
  event.xbutton.time = CurrentTime;
  event.xbutton.x = x;
  event.xbutton.y = y;
  event.xbutton.x_root = x;
  event.xbutton.y_root = y;
  event.xbutton.button = Button1;
  event.xbutton.same_screen = True;
  XSendEvent(display, appWindow, True, ButtonPressMask, &event);
  XSync(display, False);
  XFlush(display);
}

void X11App::unfocus(Window matrix) {
  focused=false;
  XSelectInput(display, matrix, 0);
  Window root = DefaultRootWindow(display);
  KeyCode eKeyCode = XKeysymToKeycode(display, XK_e);
  XUngrabKey(display, eKeyCode, AnyModifier, root);
  XSetInputFocus(display, matrix, RevertToParent, CurrentTime);
  XSync(display, False);
  XFlush(display);
}

void X11App::takeInputFocus() {
  XSetInputFocus(display, appWindow, RevertToParent, CurrentTime);
  XRaiseWindow(display, appWindow);
  XSync(display, False);
  XFlush(display);
}

int isFocusedCount = 0;
bool X11App::isFocused() {
  bool rv = focused;
  return rv;
}

void X11App::focus(Window matrix) {
  focused = true;
  Window root = DefaultRootWindow(display);
  takeInputFocus();
  KeyCode eKeyCode = XKeysymToKeycode(display, XK_e);
  XGrabKey(display, eKeyCode, Mod4Mask, root, true, GrabModeAsync, GrabModeAsync);
  XSync(display, False);
  XFlush(display);
}



void X11App::appTexture() {
  glActiveTexture(textureUnit);
  glBindTexture(GL_TEXTURE_2D, textureId);
  Pixmap pixmap = XCompositeNameWindowPixmap(display, appWindow);

  const int pixmap_attribs[] = {
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGB_EXT,
    None
  };

  GLXPixmap glxPixmap = glXCreatePixmap(display, fbConfigs[0], pixmap, pixmap_attribs);
  glXBindTexImageEXT(display, glxPixmap, GLX_FRONT_LEFT_EXT, NULL);
}

float SCREEN_WIDTH = 1920;
float SCREEN_HEIGHT = 1080;

void X11App::resize(int width, int height) {
  this->width = width;
  this->height = height;
  int x = (SCREEN_WIDTH - width) / 2;
  int y = (SCREEN_HEIGHT - height) / 2;
      XMoveResizeWindow(display, appWindow, x, y, width, height);
  XFlush(display);
  XSync(display, false);
  if(textureId != -1) {
    appTexture();
  }
}


void X11App::attachTexture(int textureUnit, int textureId) {
  this->textureUnit = textureUnit;
  this->textureId = textureId;
}
