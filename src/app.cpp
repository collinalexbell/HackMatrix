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

Display* display;
int screen;
Window emacs;
XWindowAttributes attrs;
int fbConfigCount;
GLXFBConfig* fbConfigs;

void fetchInfo() {
 display = XOpenDisplay(NULL);
  //cout << "display:" << XDisplayString(display) << endl;

  screen = XDefaultScreen(display);
  if (!gladLoadGLXLoader((GLADloadproc)glfwGetProcAddress, display, screen)) {
    std::cout << "Failed to initialize GLAD for GLX" << std::endl;
    return;
  }
  cout << screen << endl;
  Window win = XDefaultRootWindow(display);
  emacs = getWindowByName(display, "emacs@phoenix");
  XGetWindowAttributes(display, emacs, &attrs);
  cout << "width: " << attrs.width
       << ", height: " << attrs.height
       << ", depth: " << attrs.depth
       << ", screen:" << XScreenNumberOfScreen(attrs.screen) << "==" << screen
       << endl;
  // Use glXChooseFBConfig to obtain a matching GLXFBConfig
	const int pixmap_config[] = {
		GLX_BIND_TO_TEXTURE_RGBA_EXT, 1,
		GLX_BIND_TO_TEXTURE_TARGETS_EXT, GLX_TEXTURE_2D_BIT_EXT,
		GLX_RENDER_TYPE, GLX_RGBA_BIT,
		GLX_DRAWABLE_TYPE, GLX_PIXMAP_BIT,
		GLX_X_VISUAL_TYPE, GLX_TRUE_COLOR,
		GLX_X_RENDERABLE, 1,
		//GLX_FRAMEBUFFER_SRGB_CAPABLE_EXT, (GLint) GLX_DONT_CARE,
		GLX_BUFFER_SIZE, 32,
    //		GLX_SAMPLE_BUFFERS, 1,
    //		GLX_SAMPLES, 4,
		GLX_DOUBLEBUFFER, 1,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_STENCIL_SIZE, 0,
		GLX_DEPTH_SIZE, 16, 0
	};

  fbConfigs = glXChooseFBConfig(display, 0, pixmap_config, &fbConfigCount);



}

void initApp() {
  fetchInfo();
  XCompositeRedirectWindow(display, emacs, CompositeRedirectAutomatic);

}

struct ConfigAndFormat {
  GLXFBConfig config;
  int format;
};

int errorHandler(Display *dpy, XErrorEvent *err)
{
  char buf[5000];
  XGetErrorText(dpy, err->error_code, buf, 5000);
  printf("error: %s\n", buf);

  return 0;
}


void appTexture() {

  Pixmap pixmap = XCompositeNameWindowPixmap(display, emacs);
  cout << "pixmap" << pixmap << endl;

  const int pixmap_attribs[] = {
    GLX_TEXTURE_TARGET_EXT, GLX_TEXTURE_2D_EXT,
    GLX_TEXTURE_FORMAT_EXT, GLX_TEXTURE_FORMAT_RGBA_EXT,
    None
  };

  GLXPixmap glxPixmap = glXCreatePixmap(display, fbConfigs[0], pixmap, pixmap_attribs);
  glXBindTexImageEXT(display, glxPixmap, GLX_FRONT_LEFT_EXT, NULL);
}
