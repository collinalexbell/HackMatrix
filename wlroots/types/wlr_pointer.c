#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/types/wlr_pointer.h>

#include "interfaces/wlr_input_device.h"
#include "util/set.h"
#include "util/time.h"

struct wlr_pointer *wlr_pointer_from_input_device(
		struct wlr_input_device *input_device) {
	assert(input_device->type == WLR_INPUT_DEVICE_POINTER);
	return wl_container_of(input_device, (struct wlr_pointer *)NULL, base);
}

void wlr_pointer_init(struct wlr_pointer *pointer,
		const struct wlr_pointer_impl *impl, const char *name) {
	*pointer = (struct wlr_pointer){
		.impl = impl,
	};
	wlr_input_device_init(&pointer->base, WLR_INPUT_DEVICE_POINTER, name);

	wl_signal_init(&pointer->events.motion);
	wl_signal_init(&pointer->events.motion_absolute);
	wl_signal_init(&pointer->events.button);
	wl_signal_init(&pointer->events.axis);
	wl_signal_init(&pointer->events.frame);
	wl_signal_init(&pointer->events.swipe_begin);
	wl_signal_init(&pointer->events.swipe_update);
	wl_signal_init(&pointer->events.swipe_end);
	wl_signal_init(&pointer->events.pinch_begin);
	wl_signal_init(&pointer->events.pinch_update);
	wl_signal_init(&pointer->events.pinch_end);
	wl_signal_init(&pointer->events.hold_begin);
	wl_signal_init(&pointer->events.hold_end);
}

void wlr_pointer_finish(struct wlr_pointer *pointer) {
	int64_t time_msec = get_current_time_msec();
	while (pointer->button_count > 0) {
		struct wlr_pointer_button_event event = {
			.pointer = pointer,
			.time_msec = time_msec,
			.button = pointer->buttons[pointer->button_count - 1],
			.state = WL_POINTER_BUTTON_STATE_RELEASED,
		};
		wlr_pointer_notify_button(pointer, &event);
	}

	wlr_input_device_finish(&pointer->base);

	assert(wl_list_empty(&pointer->events.motion.listener_list));
	assert(wl_list_empty(&pointer->events.motion_absolute.listener_list));
	assert(wl_list_empty(&pointer->events.button.listener_list));
	assert(wl_list_empty(&pointer->events.axis.listener_list));
	assert(wl_list_empty(&pointer->events.frame.listener_list));
	assert(wl_list_empty(&pointer->events.swipe_begin.listener_list));
	assert(wl_list_empty(&pointer->events.swipe_update.listener_list));
	assert(wl_list_empty(&pointer->events.swipe_end.listener_list));
	assert(wl_list_empty(&pointer->events.pinch_begin.listener_list));
	assert(wl_list_empty(&pointer->events.pinch_update.listener_list));
	assert(wl_list_empty(&pointer->events.pinch_end.listener_list));
	assert(wl_list_empty(&pointer->events.hold_begin.listener_list));
	assert(wl_list_empty(&pointer->events.hold_end.listener_list));

	free(pointer->output_name);
}

void wlr_pointer_notify_button(struct wlr_pointer *pointer,
		struct wlr_pointer_button_event *event) {
	if (event->state == WL_POINTER_BUTTON_STATE_PRESSED) {
		set_add(pointer->buttons, &pointer->button_count,
			WLR_POINTER_BUTTONS_CAP, event->button);
	} else {
		set_remove(pointer->buttons, &pointer->button_count,
			WLR_POINTER_BUTTONS_CAP, event->button);
	}

	wl_signal_emit_mutable(&pointer->events.button, event);
}
