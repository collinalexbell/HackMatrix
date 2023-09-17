#include <iostream>
#include <X11/Xlib.h>
#include <string>

using namespace std;

void traverseWindowTree(Display* display, Window& win, void (*func)(Display*, Window&)) {
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

void printWindowName(Display* display, Window& win) {
  XWindowAttributes attrs;
  char nameMem[100];
  char* name;
  XFetchName(display, win, &name);
  if(name != NULL) {
    cout << name << endl;
  }
}

int main() {
  Display* display = XOpenDisplay(NULL);
  //cout << "display:" << XDisplayString(display) << endl;
  Window win = XDefaultRootWindow(display);
  traverseWindowTree(display, win, &printWindowName);
}
