#include <cstdlib>
#include "engine.h"
#include "wayland/wlr_compositor.h"

int main(int argc, char** argv, char** envp) {
  WlrServer server = {};
  server.envp = envp;
  server.hotkeyModifier = parse_hotkey_modifier();
  server.hotkeyModifierMask = hotkey_modifier_mask(server.hotkeyModifier);

  initialize_wlr_logging();
  apply_backend_env_defaults();

  if(!server.init_resources()) { 
    return EXIT_FAILURE; 
  }
 
  server.register_listeners();

  if(!server.start_backend_and_socket()) { return EXIT_FAILURE; }

  wl_display_run(server.display);

  return EXIT_SUCCESS;
}
