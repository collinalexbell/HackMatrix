#include <assert.h>
#include <libinput.h>
#include <wlr/interfaces/wlr_touch.h>
#include "backend/libinput.h"

const struct wlr_touch_impl libinput_touch_impl = {
	.name = "libinput-touch",
};

void init_device_touch(struct wlr_libinput_input_device *dev) {
	const char *name = get_libinput_device_name(dev->handle);
	struct wlr_touch *wlr_touch = &dev->touch;
	wlr_touch_init(wlr_touch, &libinput_touch_impl, name);

	libinput_device_get_size(dev->handle, &wlr_touch->width_mm,
		&wlr_touch->height_mm);
}

struct wlr_libinput_input_device *device_from_touch(
		struct wlr_touch *wlr_touch) {
	assert(wlr_touch->impl == &libinput_touch_impl);

	struct wlr_libinput_input_device *dev =
		wl_container_of(wlr_touch, dev, touch);
	return dev;
}

void handle_touch_down(struct libinput_event *event,
		struct wlr_touch *touch) {
	struct libinput_event_touch *tevent =
		libinput_event_get_touch_event(event);
	struct wlr_touch_down_event wlr_event = { 0 };
	wlr_event.touch = touch;
	wlr_event.time_msec =
		usec_to_msec(libinput_event_touch_get_time_usec(tevent));
	wlr_event.touch_id = libinput_event_touch_get_seat_slot(tevent);
	wlr_event.x = libinput_event_touch_get_x_transformed(tevent, 1);
	wlr_event.y = libinput_event_touch_get_y_transformed(tevent, 1);
	wl_signal_emit_mutable(&touch->events.down, &wlr_event);
}

void handle_touch_up(struct libinput_event *event,
		struct wlr_touch *touch) {
	struct libinput_event_touch *tevent =
		libinput_event_get_touch_event(event);
	struct wlr_touch_up_event wlr_event = {
		.touch = touch,
		.time_msec = usec_to_msec(libinput_event_touch_get_time_usec(tevent)),
		.touch_id = libinput_event_touch_get_seat_slot(tevent),
	};
	wl_signal_emit_mutable(&touch->events.up, &wlr_event);
}

void handle_touch_motion(struct libinput_event *event,
		struct wlr_touch *touch) {
	struct libinput_event_touch *tevent =
		libinput_event_get_touch_event(event);
	struct wlr_touch_motion_event wlr_event = {
		.touch = touch,
		.time_msec = usec_to_msec(libinput_event_touch_get_time_usec(tevent)),
		.touch_id = libinput_event_touch_get_seat_slot(tevent),
		.x = libinput_event_touch_get_x_transformed(tevent, 1),
		.y = libinput_event_touch_get_y_transformed(tevent, 1),
	};
	wl_signal_emit_mutable(&touch->events.motion, &wlr_event);
}

void handle_touch_cancel(struct libinput_event *event,
		struct wlr_touch *touch) {
	struct libinput_event_touch *tevent =
		libinput_event_get_touch_event(event);
	struct wlr_touch_cancel_event wlr_event = {
		.touch = touch,
		.time_msec = usec_to_msec(libinput_event_touch_get_time_usec(tevent)),
		.touch_id = libinput_event_touch_get_seat_slot(tevent),
	};
	wl_signal_emit_mutable(&touch->events.cancel, &wlr_event);
}

void handle_touch_frame(struct libinput_event *event,
		struct wlr_touch *touch) {
	wl_signal_emit_mutable(&touch->events.frame, NULL);
}
