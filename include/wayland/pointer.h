#pragma once

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output.h>

struct WlrServer;

void handle_new_pointer(WlrServer* server, wlr_input_device* device);
bool wayland_pointer_focus_requested(WlrServer* server);
void set_cursor_visible(WlrServer* server, bool visible, wlr_output* output = nullptr);
