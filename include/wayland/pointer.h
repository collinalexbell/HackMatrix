#pragma once

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output.h>

struct WlrServer;

// handles an event when a new physical pointer device is created
void handle_new_pointer(WlrServer* server, wlr_input_device* device);

bool wayland_pointer_focus_requested(WlrServer* server);

// used to hide and show the cursor whenever a window is focused or unfocused
void set_cursor_visible(WlrServer* server, bool visible, wlr_output* output = nullptr);

// creates the abstract pointer in wayland roots terminology
void create_cursor(WlrServer* server);
