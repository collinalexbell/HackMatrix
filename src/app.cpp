#include <iostream>
#include <X11/Xlib.h>
#include <string>

using namespace std;

int main() {
  Display* display = XOpenDisplay(NULL);
  //cout << "display:" << XDisplayString(display) << endl;
  Window target = XDefaultRootWindow(display);
  Window parent;
  Window root;
  Window childrenMem[20];
  Window *children = childrenMem;
  unsigned int nchildren_return;

  XQueryTree(display, target, &root, &parent, &children, &nchildren_return);

  XWindowAttributes attrs;
  for(int i = 0; i < nchildren_return; i++) {
    XGetWindowAttributes(display, children[i], &attrs);
    //cout << "width:" << attrs.width << ", height:" << attrs.height << ", depth:" << attrs.depth << ", isViewable:" << attrs.map_state << endl;
    char nameMem[100];
    char* name;
    XFetchName(display, children[0], &name);
    if(name != NULL) {
      cout << name << endl;
    }
  }
}
