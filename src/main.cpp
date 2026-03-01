#include <cstdlib>
#include "engine.h"
#include "wayland/wlr_compositor.h"

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
