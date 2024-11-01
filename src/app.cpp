#include <X11/X.h>
#include <X11/Xutil.h>
#include <iostream>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/XKBlib.h>
#include <optional>
#include <string>
#include <functional>
#include <glad/glad_glx.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <thread>
#include "app.h"
#include "screen.h"
#include "logger.h"
#include <sstream>

#include <X11/extensions/Xfixes.h>
#include <X11/extensions/xfixeswire.h>

using namespace std;

std::shared_ptr<spdlog::logger> app_logger;

X11App::X11App(X11App&& other) noexcept
  : display(other.display)
  , screen(other.screen)
  , appWindow(other.appWindow)
  , attrs(other.attrs)
  , fbConfigs(other.fbConfigs)
  , fbConfigCount(other.fbConfigCount)
  , textureUnit(other.textureUnit)
  , textureId(other.textureId)
  , focused(other.focused.load())
  , x(other.x)
  , y(other.y)
  , appIndex(other.appIndex)
  , width(other.width)
  , height(other.height)
{
  // Reset the source object to a valid state
  other.display = nullptr;
  other.screen = 0;
  other.appWindow = 0;
  other.fbConfigs = nullptr;
  other.fbConfigCount = 0;
  other.textureUnit = -1;
  other.textureId = -1;
  other.x = 0;
  other.y = 0;
  other.appIndex = 0;
  other.width = 0;
  other.height = 0;
}

bool
X11App::initAppClass(Display* display, int screen)
{
  app_logger = make_shared<spdlog::logger>("app", fileSink);
  app_logger->set_level(spdlog::level::info);
  if (!gladLoadGLXLoader((GLADloadproc)glfwGetProcAddress, display, screen)) {
    std::cout << "Failed to initialize GLAD for GLX" << std::endl;
    app_logger->error("Failed to initialize GLAD for GLX");
    return false;
  }
  return true;
};

/*
const int pixmap_config[] = {
  GLX_RGBA,1,
  GLX_DOUBLEBUFFER, 1,
  GLX_BIND_TO_TEXTURE_RGBA_EXT, 1,
  GLX_RENDER_TYPE, GLX_RGBA_BIT,
  0x20B2, (GLint) GLX_DONT_CARE,
  GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
  GLX_X_RENDERABLE, true,
  GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
  GLX_BUFFER_SIZE, 32,
  GLX_RED_SIZE, 8,
  GLX_GREEN_SIZE, 8,
  GLX_BLUE_SIZE, 8,
  GLX_ALPHA_SIZE, 8,
  GLX_STENCIL_SIZE, 0,
  GLX_DEPTH_SIZE, 24,
  0};
*/

const int pixmap_config[] = { GLX_BIND_TO_TEXTURE_RGBA_EXT,
                              1,
                              GLX_BIND_TO_TEXTURE_TARGETS_EXT,
                              GLX_TEXTURE_2D_BIT_EXT,
                              GLX_RENDER_TYPE,
                              GLX_RGBA_BIT,
                              GLX_DRAWABLE_TYPE,
                              GLX_PIXMAP_BIT,
                              GLX_X_VISUAL_TYPE,
                              GLX_TRUE_COLOR,
                              GLX_X_RENDERABLE,
                              1,
                              GLX_BUFFER_SIZE,
                              32,
                              //		GLX_SAMPLE_BUFFERS, 1,
                              //		GLX_SAMPLES, 4,
                              GLX_DOUBLEBUFFER,
                              1,
                              GLX_RED_SIZE,
                              8,
                              GLX_GREEN_SIZE,
                              8,
                              GLX_BLUE_SIZE,
                              8,
                              GLX_ALPHA_SIZE,
                              8,
                              GLX_STENCIL_SIZE,
                              0,
                              GLX_DEPTH_SIZE,
                              16,
                              0 };

std::vector<int>
getWindowRootPosition(Display* display, Window window)
{
  Window root_return, parent_return, *children_return;
  unsigned int num_children_return;
  int x_root = 0;
  int y_root = 0;

  // Start with the given window
  Window currentWindow = window;

  // Traverse up the hierarchy until we reach the root window
  while (currentWindow != 0) {
    int x_child, y_child;

    // Get the position of the current window relative to its parent
    XTranslateCoordinates(display,
                          currentWindow,
                          DefaultRootWindow(display),
                          0,
                          0,
                          &x_child,
                          &y_child,
                          &window);

    x_root += x_child;
    y_root += y_child;

    // Move up to the parent window
    XQueryTree(display,
               currentWindow,
               &root_return,
               &parent_return,
               &children_return,
               &num_children_return);

    // Free the children list (if any)
    if (children_return) {
      XFree(children_return);
    }

    currentWindow = parent_return;
  }

  // Return the final coordinates relative to the root window
  return { x_root, y_root };
}

void
traverseWindowTree(Display* display,
                   Window win,
                   std::function<void(Display*, Window)> func)
{
  func(display, win);

  // handle children
  Window parent;
  Window root;
  Window childrenMem[20];
  Window* children = childrenMem;
  unsigned int nchildren_return;

  XQueryTree(display, win, &root, &parent, &children, &nchildren_return);

  for (int i = 0; i < nchildren_return; i++) {
    traverseWindowTree(display, children[i], func);
  }
}

std::optional<int>
getPID(Display* display, Window window)
{
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char* prop = NULL;
  pid_t pid;

  // Get the _NET_WM_PID atom
  auto net_wm_pid = XInternAtom(display, "_NET_WM_PID", False);

  // Get the window property
  if (XGetWindowProperty(display,
                         window,
                         net_wm_pid,
                         0,
                         LONG_MAX,
                         False,
                         XA_CARDINAL,
                         &actual_type,
                         &actual_format,
                         &nitems,
                         &bytes_after,
                         &prop) == Success) {
    if (actual_format == 32 && actual_type == XA_CARDINAL && nitems > 0) {
      pid = *((pid_t*)prop);
      XFree(prop);
      return pid;
    }
  }
  return nullopt;
}

Window
getWindowByPid(Display* display, int pid)
{
  Window root = XDefaultRootWindow(display);
  Window rv;
  bool found = false;
  int largestWidth = 0;

  traverseWindowTree(
    display,
    root,
    [&rv, &found, &largestWidth, pid](Display* display, Window window) {
      if (pid == getPID(display, window)) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, window, &attrs);
        bool larger = attrs.width > largestWidth;
        bool equalAndNotAccessory =
          attrs.width == largestWidth && !attrs.override_redirect;
        if (larger || equalAndNotAccessory) {
          largestWidth = attrs.width;
          rv = window;
          found = true;
        }
      }
    });

  if (!found) {
    cout << "unable to find window with pid: \"" << pid << "\"" << endl;
    throw "unable to find window";
  }
  return rv;
}

Window
getWindowByName(Display* display, string search)
{
  Window root = XDefaultRootWindow(display);
  Window rv;
  bool found = false;

  traverseWindowTree(
    display, root, [&rv, &found, search](Display* display, Window window) {
      char nameMem[100];
      char* name = nameMem;
      XFetchName(display, window, &name);
      if (name != NULL && string(name).find(search) != string::npos) {
        found = true;
        rv = window;
      }
    });

  if (!found) {
    cout << "unable to find window: \"" << search << "\"" << endl;
    throw "unable to find window";
  }
  return rv;
}

Window
getWindowByClass(Display* display, string search)
{
  Window root = XDefaultRootWindow(display);
  Window rv;
  int largestWidth = 0;
  bool found = false;

  traverseWindowTree(
    display,
    root,
    [&rv, &found, search, &largestWidth](Display* display, Window window) {
      XClassHint classHint;
      classHint.res_class = NULL;
      classHint.res_name = NULL;
      char* className = NULL;
      XGetClassHint(display, window, &classHint);
      className = classHint.res_class;
      if (className != NULL && string(className).find(search) != string::npos) {
        XWindowAttributes attrs;
        XGetWindowAttributes(display, window, &attrs);
        bool larger = attrs.width > largestWidth;
        bool equalAndNotAccessory =
          attrs.width == largestWidth && !attrs.override_redirect;
        if (larger || equalAndNotAccessory) {
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

void
X11App::fetchInfo(Identifier identifier)
{
  switch (identifier.type) {
    case NAME:
      appWindow = getWindowByName(display, identifier.strId);
      break;
    case CLASS:
      appWindow = getWindowByClass(display, identifier.strId);
      break;
    case WINDOW:
      appWindow = identifier.win;
      break;
    case PID:
      appWindow = getWindowByPid(display, identifier.pid);
  }
  app_logger->info("XMapWindow()");
  app_logger->flush();
  XMapWindow(display, appWindow);
  XFlush(display);
  XSync(display, False);
  app_logger->info("XGetWindowAttributes()");
  app_logger->flush();
  XGetWindowAttributes(display, appWindow, &attrs);
  fbConfigs = glXChooseFBConfig(display, 0, pixmap_config, &fbConfigCount);
}

int
errorHandler(Display* dpy, XErrorEvent* err)
{
  char buf[5000];
  XGetErrorText(dpy, err->error_code, buf, 5000);
  printf("error: %s\n", buf);
  std::stringstream ss;
  ss << buf << " : requestcode:" << (unsigned int)err->request_code;
  app_logger->error(ss.str());
  app_logger->flush();

  return 0;
}

X11App::X11App(Display* display, int screen)
  : display(display)
  , screen(screen)
{
  XSetErrorHandler(errorHandler);
};

X11App*
X11App::byName(string windowName,
               Display* display,
               int screen,
               int width,
               int height)
{
  X11App* rv = new X11App(display, screen);
  Identifier id;
  id.type = NAME;
  id.strId = windowName;
  rv->fetchInfo(id);
  rv->resize(width, height);
  return rv;
}

X11App*
X11App::byClass(string windowClass,
                Display* display,
                int screen,
                int width,
                int height)
{
  X11App* rv = new X11App(display, screen);
  Identifier id;
  id.type = CLASS;
  id.strId = windowClass;
  rv->fetchInfo(id);
  rv->resize(width, height);
  return rv;
}

X11App*
X11App::byWindow(Window window,
                 Display* display,
                 int screen,
                 int width,
                 int height)
{
  X11App* rv = new X11App(display, screen);
  Identifier id;
  id.type = WINDOW;
  id.win = window;
  rv->fetchInfo(id);
  rv->resize(width, height);
  return rv;
}

X11App*
X11App::byPID(int pid, Display* display, int screen, int width, int height)
{
  X11App* rv = new X11App(display, screen);
  Identifier id;
  id.type = PID;
  id.pid = pid;
  rv->fetchInfo(id);
  rv->resize(width, height);
  return rv;
}

void
X11App::unfocus(Window matrix)
{
  focused = false;
  XSelectInput(display, matrix, 0);
  Window root = DefaultRootWindow(display);
  KeyCode eKeyCode = XKeysymToKeycode(display, XK_e);
  XUngrabKey(display, eKeyCode, AnyModifier, root);
  KeyCode bKeyCode = XKeysymToKeycode(display, XK_b);
  XUngrabKey(display, bKeyCode, AnyModifier, root);

  XSetInputFocus(display, matrix, RevertToParent, CurrentTime);
  XSync(display, False);
  XFlush(display);
}

void
X11App::takeInputFocus()
{
  XSetInputFocus(display, appWindow, RevertToParent, CurrentTime);
  XRaiseWindow(display, appWindow);
  XSync(display, False);
  XFlush(display);
}

int isFocusedCount = 0;
bool
X11App::isFocused()
{
  bool rv = focused;
  return rv;
}

void
X11App::focus(Window matrix)
{
  focused = true;
  Window root = DefaultRootWindow(display);

  // Set _NET_SUPPORTING_WM_CHECK property on the root window
  Atom net_supporting_wm_check =
    XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", False);
  XChangeProperty(display,
                  RootWindow(display, DefaultScreen(display)),
                  net_supporting_wm_check,
                  XA_WINDOW,
                  32,
                  PropModeReplace,
                  (unsigned char*)&appWindow,
                  1);

  takeInputFocus();
  KeyCode eKeyCode = XKeysymToKeycode(display, XK_e);
  XGrabKey(
    display, eKeyCode, Mod4Mask, root, true, GrabModeAsync, GrabModeAsync);
  KeyCode bKeyCode = XKeysymToKeycode(display, XK_b);
  XGrabKey(
    display, bKeyCode, Mod4Mask, root, true, GrabModeAsync, GrabModeAsync);

  XSync(display, False);
  XFlush(display);
}

void
X11App::appTexture()
{
  glActiveTexture(textureUnit);
  glBindTexture(GL_TEXTURE_2D, textureId);

  app_logger->info("XCompositeNameWindowPixmap()");
  app_logger->flush();
  Pixmap pixmap = XCompositeNameWindowPixmap(display, appWindow);

  const int pixmap_attribs[] = { GLX_TEXTURE_TARGET_EXT,
                                 GLX_TEXTURE_2D_EXT,
                                 GLX_TEXTURE_FORMAT_EXT,
                                 GLX_TEXTURE_FORMAT_RGBA_EXT,
                                 None };

  int i = 0;
  for (; i < fbConfigCount; i++) {
    auto config = fbConfigs[i];

    int has_alpha;
    glad_glXGetFBConfigAttrib(
      display, config, GLX_BIND_TO_TEXTURE_RGBA_EXT, &has_alpha);
    if (has_alpha) {
      cout << "HAS ALPHA" << endl;
      break;
    }
  }

  app_logger->info("glXCreatePixmap()");
  app_logger->flush();
  GLXPixmap glxPixmap =
    glXCreatePixmap(display, fbConfigs[i], pixmap, pixmap_attribs);
  app_logger->info("glXBindTexImageEXT()");
  app_logger->flush();

  // clear errors
  glGetError();
  glXBindTexImageEXT(display, glxPixmap, GLX_FRONT_LEFT_EXT, NULL);
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    throw "failed to bind tex image (bad pixmap)";
  }
  app_logger->info("appTexture() success");
}

void
getAbsoluteMousePosition(Display* display, int* x_out, int* y_out)
{
  Window root_window = DefaultRootWindow(display);
  Window returned_root, returned_child;
  int root_x, root_y, win_x, win_y;
  unsigned int mask;

  Bool result = XQueryPointer(display,
                              root_window,
                              &returned_root,
                              &returned_child,
                              &root_x,
                              &root_y,
                              &win_x,
                              &win_y,
                              &mask);

  if (result == True) {
    *x_out = win_x;
    *y_out = win_y;
  } else {
    *x_out = 0;
    *y_out = 0;
  }
}

void
X11App::resizeMove(int width, int height, int x, int y)
{
  this->width = width;
  this->height = height;
  this->x = x;
  this->y = SCREEN_HEIGHT - y - height;
  if (!isAccessory()) {
    XMoveResizeWindow(display, appWindow, x, y, width, height);
    XFlush(display);
  }
}

std::pair<int, int>
getWindowPosition(Window window)
{
  std::stringstream ss;
  ss << "0x" << std::hex << window;
  std::string windowId = ss.str();

  std::string command = "xwininfo -id " + windowId;
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) {
    std::cerr << "Failed to execute xwininfo command." << std::endl;
    return std::make_pair(-1, -1);
  }

  char buffer[128];
  std::string output;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
  }
  pclose(pipe);

  std::string searchString = "Absolute upper-left X:";
  size_t pos = output.find(searchString);
  if (pos != std::string::npos) {
    pos += searchString.length();
    int x = std::stoi(output.substr(pos));

    searchString = "Absolute upper-left Y:";
    pos = output.find(searchString);
    if (pos != std::string::npos) {
      pos += searchString.length();
      int y = std::stoi(output.substr(pos));

      return std::make_pair(x, y);
    }
  }

  std::cerr << "Failed to parse xwininfo output." << std::endl;
  return std::make_pair(-1, -1);
}

void
X11App::resize(int width, int height)
{
  this->width = width;
  this->height = height;
  bool resized = false;
  XWindowAttributes attributes;
  XGetWindowAttributes(display, appWindow, &attributes);
  if (attributes.width != width || attributes.height != height) {
    resized = true;
  }
  if (!isAccessory()) {
    x = (SCREEN_WIDTH - width) / 2;
    y = (SCREEN_HEIGHT - height) / 2;
    if (attributes.x != x || attributes.y != y || attributes.width != width ||
        attributes.height != height) {
      XMoveResizeWindow(display, appWindow, x, y, width, height);
    }
  } else {
    auto position = getWindowPosition(appWindow);
    int newX;
    int newY;
    if (position.first != -1 && position.second != -1) {
      newX = position.first;
      newY = position.second;
    } else {
      newX = attributes.x;
      newY = attributes.y;
    }
    int invertedY = SCREEN_HEIGHT - newY;
    invertedY -= height;

    if (newX != x || invertedY != y || attributes.width != width ||
        attributes.height != height) {
      x = newX;
      y = invertedY;
      // XMoveResizeWindow(display, appWindow, x,y, width, height);
    }
  }
  if (resized) {
    XFlush(display);
    XSync(display, false);
    if (textureId != -1) {
      try {
        appTexture();
      } catch (...) {
      }
    }
  }
}

Window
X11App::getWindow()
{
  return appWindow;
}

void
X11App::positionNotify(int x, int y)
{
  this->x = x;
  this->y = y;
  XMoveWindow(display, appWindow, x, y);
}

array<int, 2>
X11App::getPosition()
{
  return { x, y };
}

void
X11App::attachTexture(int textureUnit, int textureId, size_t appIndex)
{
  this->textureUnit = textureUnit;
  this->textureId = textureId;
  this->appIndex = appIndex;
}

bool
X11App::isAccessory()
{
  XWindowAttributes attrs;

  glGetError();
  XGetWindowAttributes(display, appWindow, &attrs);
  GLenum error = glGetError();
  if (error != GL_NO_ERROR) {
    throw "XGetWindowAttributes failed";
  }
  if (attrs.override_redirect) {
    return true;
  }
  return false;
}

int
X11App::getPID()
{
  auto pid = ::getPID(display, appWindow);
  if (pid.has_value()) {
    return pid.value();
  } else {
    return -1;
  }
}

string
X11App::getWindowName()
{
  char* name;
  XFetchName(display, appWindow, &name);
  if (name != NULL) {
    return string(name);
  }
  return "";
}

bool
X11App::isSelected()
{
  return selected;
}

void
X11App::select()
{
  selected = true;
}

void
X11App::deselect()
{
  selected = false;
}
