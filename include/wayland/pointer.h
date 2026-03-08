#pragma once

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>

struct WlrServer;

void handle_new_pointer(WlrServer* server, wlr_input_device* device);

void handle_pointer_motion(wl_listener* listener, void* data);
void handle_pointer_motion_abs(wl_listener* listener, void* data);
void handle_pointer_axis(wl_listener* listener, void* data);
void handle_pointer_button(wl_listener* listener, void* data);
void handle_pointer_destroy(wl_listener* listener, void* data);
bool wayland_pointer_focus_requested(WlrServer* server);
