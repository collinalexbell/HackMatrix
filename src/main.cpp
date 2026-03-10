#include <cstdlib>
#include "screen.h"
#include "engine.h"
#include "wayland/wlr_compositor.h"

// just initialize, server will take care of actual init
float SCREEN_WIDTH = 0;
float SCREEN_HEIGHT = 0;

int main(int argc, char** argv, char** envp) {
  WlrServer server = WlrServer(envp);

  if(!server.init_resources()) { 
    return EXIT_FAILURE; 
  }
 
  if(!server.start()) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
