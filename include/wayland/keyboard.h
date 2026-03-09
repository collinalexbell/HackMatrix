#pragma once

#include <wayland-server-core.h>
#include <wlr/types/wlr_input_device.h>

struct WlrServer;

void handle_new_keyboard(WlrServer* server, wlr_input_device* device);
