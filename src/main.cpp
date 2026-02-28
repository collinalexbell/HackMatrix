#include <cstdlib>
#include "engine.h"
#include "wayland/wlr_compositor.h"

int main(int argc, char** argv, char** envp) {
  WlrServer server = {};
  server.envp = envp;
  server.hotkeyModifier = parse_hotkey_modifier();
  server.hotkeyModifierMask = hotkey_modifier_mask(server.hotkeyModifier);

  initialize_wlr_logging();

  write_pid_for_kill();
  apply_backend_env_defaults();

  if(!server.create_display() || !server.create_backend()) {
    return EXIT_FAILURE;
  }

  if (!server.create_renderer() || !server.create_allocator()) {
    teardown_server(server);
    return EXIT_FAILURE;
  }

  if (!init_protocols_and_seat(server)) {
    teardown_server(server);
    return EXIT_FAILURE;
  }
  register_global_listeners(server);

  if (!start_backend_and_socket(server)) {
    teardown_server(server);
    return EXIT_FAILURE;
  }

  wl_display_run(server.display);

  teardown_server(server);

  return EXIT_SUCCESS;
}
