#pragma once

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_output.h>
#include <utility>

struct WlrServer;
struct wlr_surface;

// handles an event when a new physical pointer device is created
void handle_new_pointer(WlrServer* server, wlr_input_device* device);

bool wayland_pointer_focus_requested(WlrServer* server);
bool wayland_pointer_locked(WlrServer* server);
void update_pointer_constraint(WlrServer* server, wlr_surface* focused_surface);
void sync_cursor_mode_pointer_focus(WlrServer* server);
void clear_cursor_mode_input_focus(WlrServer* server);

// wlr_cursor positions are in output-layout coordinates. Renderer and client
// surface coordinates are local to one output.
std::pair<double, double> output_local_pointer(WlrServer* server,
                                               wlr_output* output);

// used to hide and show the cursor whenever a window is focused or unfocused
void set_cursor_visible(WlrServer* server, bool visible, wlr_output* output = nullptr);

// creates the abstract pointer in wayland roots terminology
void create_cursor(WlrServer* server);
