#include <iostream>
#include <X11/Xlib.h>
#include <X11/extensions/Xcomposite.h>
#include <string>
#include <functional>
#include <glad/glad_glx.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

using namespace std;

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

  traverseWindowTree(display, root, [&rv, search](Display* display, Window window) {
    char nameMem[100];
    char* name;
    XFetchName(display, window, &name);
    if(name != NULL && string(name) == search) {
      rv = window;
    }
  });

  return rv;
}

void printWindowName(Display* display, Window win) {
  XWindowAttributes attrs;
  char nameMem[100];
  char* name;
  XFetchName(display, win, &name);
  if(name != NULL) {
    cout << name << endl;
  }
}


void runApp() {
  Display* display = XOpenDisplay(NULL);
  //cout << "display:" << XDisplayString(display) << endl;

  int screen = XDefaultScreen(display);
  if (!gladLoadGLXLoader((GLADloadproc)glfwGetProcAddress, display, screen)) {
    std::cout << "Failed to initialize GLAD for GLX" << std::endl;
    return;
  }
  cout << screen << endl;
  Window win = XDefaultRootWindow(display);
  Window emacs = getWindowByName(display, "emacs@phoenix");
  XWindowAttributes attrs;
  XGetWindowAttributes(display, emacs, &attrs);
  cout << "width: " << attrs.width
       << ", height" << attrs.height
       << endl;

  int attribs[] = {
    GLX_RGBA,     // Use an RGBA visual
    GLX_DEPTH_SIZE, 24,  // Desired depth buffer size
    None  // End of attribute list
  };

  // Use glXChooseFBConfig to obtain a matching GLXFBConfig
  int fbConfigCount;
  GLXFBConfig* fbConfigs = glXChooseFBConfig(display, screen, attribs, &fbConfigCount);

  Pixmap pixmap = XCompositeNameWindowPixmap(display, emacs);
  //GLXPixmap glxPixmap = glXCreatePixmap(display, fbConfigs[0], pixmap, pixmap_attributes);
  //glXBindTexImageEXT(display, glxPixmap, GLX_FRONT_LEFT_EXT, NULL);
}
