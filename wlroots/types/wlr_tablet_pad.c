#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/util/log.h>

#include "interfaces/wlr_input_device.h"

struct wlr_tablet_pad *wlr_tablet_pad_from_input_device(
		struct wlr_input_device *input_device) {
	assert(input_device->type == WLR_INPUT_DEVICE_TABLET_PAD);
	return wl_container_of(input_device, (struct wlr_tablet_pad *)NULL, base);
}

void wlr_tablet_pad_init(struct wlr_tablet_pad *pad,
		const struct wlr_tablet_pad_impl *impl, const char *name) {
	*pad = (struct wlr_tablet_pad){
		.impl = impl,
	};
	wlr_input_device_init(&pad->base, WLR_INPUT_DEVICE_TABLET_PAD, name);

	wl_signal_init(&pad->events.button);
	wl_signal_init(&pad->events.ring);
	wl_signal_init(&pad->events.strip);
	wl_signal_init(&pad->events.attach_tablet);

	wl_list_init(&pad->groups);
	wl_array_init(&pad->paths);
}

void wlr_tablet_pad_finish(struct wlr_tablet_pad *pad) {
	wlr_input_device_finish(&pad->base);

	assert(wl_list_empty(&pad->events.button.listener_list));
	assert(wl_list_empty(&pad->events.ring.listener_list));
	assert(wl_list_empty(&pad->events.strip.listener_list));
	assert(wl_list_empty(&pad->events.attach_tablet.listener_list));

	char **path_ptr;
	wl_array_for_each(path_ptr, &pad->paths) {
		free(*path_ptr);
	}
	wl_array_release(&pad->paths);

	/* TODO: wlr_tablet_pad should own its wlr_tablet_pad_group */
	if (!wl_list_empty(&pad->groups)) {
		wlr_log(WLR_ERROR, "wlr_tablet_pad groups is not empty");
	}
}
