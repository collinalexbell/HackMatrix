#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_switch.h>
#include <wlr/types/wlr_switch.h>

#include "interfaces/wlr_input_device.h"

struct wlr_switch *wlr_switch_from_input_device(
		struct wlr_input_device *input_device) {
	assert(input_device->type == WLR_INPUT_DEVICE_SWITCH);
	return wl_container_of(input_device, (struct wlr_switch *)NULL, base);
}

void wlr_switch_init(struct wlr_switch *switch_device,
		const struct wlr_switch_impl *impl, const char *name) {
	*switch_device = (struct wlr_switch){
		.impl = impl,
	};
	wlr_input_device_init(&switch_device->base, WLR_INPUT_DEVICE_SWITCH, name);

	wl_signal_init(&switch_device->events.toggle);
}

void wlr_switch_finish(struct wlr_switch *switch_device) {
	wlr_input_device_finish(&switch_device->base);

	assert(wl_list_empty(&switch_device->events.toggle.listener_list));
}
