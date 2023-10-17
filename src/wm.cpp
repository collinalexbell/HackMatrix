#include "wm.h"
#include "app.h"
#include <glm/glm.hpp>

#include <X11/extensions/shape.h>

int APP_WIDTH = 1920 * .85;
int APP_HEIGHT = 1920 * .85 * .54;
void WM::forkOrFindApp(string cmd, string pidOf, string className, X11App *&app, char **envp) {
  char *line;
  std::size_t len = 0;
  FILE *pidPipe = popen(string("pidof " + pidOf).c_str(), "r");
  if (getline(&line, &len, pidPipe) == -1) {
    int pid = fork();
    if (pid == 0) {
      setsid();
      execle(cmd.c_str(), cmd.c_str(), NULL, envp);
      exit(0);
    }
    if (className == "obs") {
      sleep(30);
    } else {
      sleep(1);
    }
  }
  app = X11App::byClass(className, display, screen, APP_WIDTH, APP_HEIGHT);
}

void WM::createAndRegisterApps(char **envp) {
  forkOrFindApp("/usr/bin/emacs", "emacs", "Emacs", emacs, envp);
  forkOrFindApp("/usr/bin/microsoft-edge", "microsoft-edge", "Microsoft-edge",
                microsoftEdge, envp);
  forkOrFindApp("/usr/bin/terminator", "terminator", "Terminator", terminator,
                envp);
  forkOrFindApp("/usr/bin/obs", "obs", "obs", obs, envp);
}


void WM::allow_input_passthrough(Window window) {
  XserverRegion region = XFixesCreateRegion(display, NULL, 0);

  XFixesSetWindowShapeRegion(display, window, ShapeBounding, 0, 0, 0);
  XFixesSetWindowShapeRegion(display, window, ShapeInput, 0, 0, region);

  XFixesDestroyRegion(display, region);
}

void WM::addAppsToWorld() {
  world->addApp(glm::vec3(2.8, 1.0, 5.0), terminator);
  world->addApp(glm::vec3(4.0, 1.0, 5.0), emacs);
  world->addApp(glm::vec3(4.0, 2.0, 5.0), microsoftEdge);
  world->addApp(glm::vec3(5.2, 1.0, 5.0), obs);
}

WM::WM(Window overlay, Window matrix, Display *display, int screen) : display(display), screen(screen) {
  allow_input_passthrough(overlay);
  allow_input_passthrough(matrix);
}
