#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_tablet_tool.h>
#include <wlr/types/wlr_tablet_tool.h>

#include "interfaces/wlr_input_device.h"

struct wlr_tablet *wlr_tablet_from_input_device(
		struct wlr_input_device *input_device) {
	assert(input_device->type == WLR_INPUT_DEVICE_TABLET);
	return wl_container_of(input_device, (struct wlr_tablet *)NULL, base);
}

void wlr_tablet_init(struct wlr_tablet *tablet,
		const struct wlr_tablet_impl *impl, const char *name) {
	*tablet = (struct wlr_tablet){
		.impl = impl,
	};
	wlr_input_device_init(&tablet->base, WLR_INPUT_DEVICE_TABLET, name);

	wl_signal_init(&tablet->events.axis);
	wl_signal_init(&tablet->events.proximity);
	wl_signal_init(&tablet->events.tip);
	wl_signal_init(&tablet->events.button);

	wl_array_init(&tablet->paths);
}

void wlr_tablet_finish(struct wlr_tablet *tablet) {
	wlr_input_device_finish(&tablet->base);

	assert(wl_list_empty(&tablet->events.axis.listener_list));
	assert(wl_list_empty(&tablet->events.proximity.listener_list));
	assert(wl_list_empty(&tablet->events.tip.listener_list));
	assert(wl_list_empty(&tablet->events.button.listener_list));

	char **path_ptr;
	wl_array_for_each(path_ptr, &tablet->paths) {
		free(*path_ptr);
	}
	wl_array_release(&tablet->paths);
}
