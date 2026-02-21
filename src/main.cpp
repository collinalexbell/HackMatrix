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

  if (!create_display_and_backend(server)) {
    return EXIT_FAILURE;
  }
  if (!create_renderer_and_allocator(server)) {
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

  install_sigint_handler();

  wl_display_run(server.display);

  teardown_server(server);

  return EXIT_SUCCESS;
}
