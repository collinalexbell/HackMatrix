/*
x-window-list - simple window-list (like xwininfo -root) for no-wm
Written in 2010 by Patrick Haller

To the extent possible under law, the author(s) have dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide. This software is distributed without any warranty. You should have
received a copy of the CC0 Public Domain Dedication along with this software.
If not, see http://creativecommons.org/publicdomain/zero/1.0/ */

// http://tronche.com/gui/x/xlib/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>


int main(int argc, char **argv) {
    Display *dpy;

    if ((dpy = XOpenDisplay(NULL)) == NULL)
      return 1;

    if (argc > 1) {
      Window w = atoi(argv[1]);
      printf("0x%-12x", w);
      XRaiseWindow(dpy, w);
      XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
      XFlush(dpy);
    }

    return 0;
}
