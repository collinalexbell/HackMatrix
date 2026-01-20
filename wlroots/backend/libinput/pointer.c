#include <assert.h>
#include <libinput.h>
#include <wlr/interfaces/wlr_pointer.h>
#include "backend/libinput.h"

const struct wlr_pointer_impl libinput_pointer_impl = {
	.name = "libinput-pointer",
};

void init_device_pointer(struct wlr_libinput_input_device *dev) {
	const char *name = get_libinput_device_name(dev->handle);
	struct wlr_pointer *wlr_pointer = &dev->pointer;
	wlr_pointer_init(wlr_pointer, &libinput_pointer_impl, name);
}

struct wlr_libinput_input_device *device_from_pointer(
		struct wlr_pointer *wlr_pointer) {
	assert(wlr_pointer->impl == &libinput_pointer_impl);

	struct wlr_libinput_input_device *dev =
		wl_container_of(wlr_pointer, dev, pointer);
	return dev;
}

void handle_pointer_motion(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_pointer *pevent =
		libinput_event_get_pointer_event(event);
	struct wlr_pointer_motion_event wlr_event = {
		.pointer = pointer,
		.time_msec = usec_to_msec(libinput_event_pointer_get_time_usec(pevent)),
		.delta_x = libinput_event_pointer_get_dx(pevent),
		.delta_y = libinput_event_pointer_get_dy(pevent),
		.unaccel_dx = libinput_event_pointer_get_dx_unaccelerated(pevent),
		.unaccel_dy = libinput_event_pointer_get_dy_unaccelerated(pevent),
	};
	wl_signal_emit_mutable(&pointer->events.motion, &wlr_event);
	wl_signal_emit_mutable(&pointer->events.frame, pointer);
}

void handle_pointer_motion_abs(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_pointer *pevent =
		libinput_event_get_pointer_event(event);
	struct wlr_pointer_motion_absolute_event wlr_event = {
		.pointer = pointer,
		.time_msec = usec_to_msec(libinput_event_pointer_get_time_usec(pevent)),
		.x = libinput_event_pointer_get_absolute_x_transformed(pevent, 1),
		.y = libinput_event_pointer_get_absolute_y_transformed(pevent, 1),
	};
	wl_signal_emit_mutable(&pointer->events.motion_absolute, &wlr_event);
	wl_signal_emit_mutable(&pointer->events.frame, pointer);
}

void handle_pointer_button(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_pointer *pevent =
		libinput_event_get_pointer_event(event);
	struct wlr_pointer_button_event wlr_event = {
		.pointer = pointer,
		.time_msec = usec_to_msec(libinput_event_pointer_get_time_usec(pevent)),
		.button = libinput_event_pointer_get_button(pevent),
	};
	switch (libinput_event_pointer_get_button_state(pevent)) {
	case LIBINPUT_BUTTON_STATE_PRESSED:
		wlr_event.state = WL_POINTER_BUTTON_STATE_PRESSED;
		break;
	case LIBINPUT_BUTTON_STATE_RELEASED:
		wlr_event.state = WL_POINTER_BUTTON_STATE_RELEASED;
		break;
	}
	wlr_pointer_notify_button(pointer, &wlr_event);
	wl_signal_emit_mutable(&pointer->events.frame, pointer);
}

void handle_pointer_axis(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_pointer *pevent =
		libinput_event_get_pointer_event(event);
	struct wlr_pointer_axis_event wlr_event = {
		.pointer = pointer,
		.time_msec = usec_to_msec(libinput_event_pointer_get_time_usec(pevent)),
	};
	switch (libinput_event_pointer_get_axis_source(pevent)) {
	case LIBINPUT_POINTER_AXIS_SOURCE_WHEEL:
		wlr_event.source = WL_POINTER_AXIS_SOURCE_WHEEL;
		break;
	case LIBINPUT_POINTER_AXIS_SOURCE_FINGER:
		wlr_event.source = WL_POINTER_AXIS_SOURCE_FINGER;
		break;
	case LIBINPUT_POINTER_AXIS_SOURCE_CONTINUOUS:
		wlr_event.source = WL_POINTER_AXIS_SOURCE_CONTINUOUS;
		break;
	case LIBINPUT_POINTER_AXIS_SOURCE_WHEEL_TILT:
		wlr_event.source = WL_POINTER_AXIS_SOURCE_WHEEL_TILT;
		break;
	}
	const enum libinput_pointer_axis axes[] = {
		LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL,
		LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL,
	};
	for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); ++i) {
		if (!libinput_event_pointer_has_axis(pevent, axes[i])) {
			continue;
		}

		switch (axes[i]) {
		case LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL:
			wlr_event.orientation = WL_POINTER_AXIS_VERTICAL_SCROLL;
			break;
		case LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL:
			wlr_event.orientation = WL_POINTER_AXIS_HORIZONTAL_SCROLL;
			break;
		}
		wlr_event.delta =
			libinput_event_pointer_get_axis_value(pevent, axes[i]);
		wlr_event.delta_discrete =
			libinput_event_pointer_get_axis_value_discrete(pevent, axes[i]);
		wlr_event.delta_discrete *= WLR_POINTER_AXIS_DISCRETE_STEP;
		wlr_event.relative_direction = WL_POINTER_AXIS_RELATIVE_DIRECTION_IDENTICAL;
		if (libinput_device_config_scroll_get_natural_scroll_enabled(libinput_event_get_device(event))) {
			wlr_event.relative_direction = WL_POINTER_AXIS_RELATIVE_DIRECTION_INVERTED;
		}
		wl_signal_emit_mutable(&pointer->events.axis, &wlr_event);
	}
	wl_signal_emit_mutable(&pointer->events.frame, pointer);
}

void handle_pointer_axis_value120(struct libinput_event *event,
		struct wlr_pointer *pointer, enum wl_pointer_axis_source source) {
	struct libinput_event_pointer *pevent =
		libinput_event_get_pointer_event(event);
	struct wlr_pointer_axis_event wlr_event = {
		.pointer = pointer,
		.time_msec = usec_to_msec(libinput_event_pointer_get_time_usec(pevent)),
		.source = source,
	};

	const enum libinput_pointer_axis axes[] = {
		LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL,
		LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL,
	};
	for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); ++i) {
		if (!libinput_event_pointer_has_axis(pevent, axes[i])) {
			continue;
		}
		switch (axes[i]) {
		case LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL:
			wlr_event.orientation = WL_POINTER_AXIS_VERTICAL_SCROLL;
			break;
		case LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL:
			wlr_event.orientation = WL_POINTER_AXIS_HORIZONTAL_SCROLL;
			break;
		}
		wlr_event.delta =
			libinput_event_pointer_get_scroll_value(pevent, axes[i]);
		if (source == WL_POINTER_AXIS_SOURCE_WHEEL) {
			wlr_event.delta_discrete =
				libinput_event_pointer_get_scroll_value_v120(pevent, axes[i]);
		}
		wl_signal_emit_mutable(&pointer->events.axis, &wlr_event);
	}
	wl_signal_emit_mutable(&pointer->events.frame, pointer);
}

void handle_pointer_swipe_begin(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_gesture *gevent =
		libinput_event_get_gesture_event(event);
	struct wlr_pointer_swipe_begin_event wlr_event = {
		.pointer = pointer,
		.time_msec =
			usec_to_msec(libinput_event_gesture_get_time_usec(gevent)),
		.fingers = libinput_event_gesture_get_finger_count(gevent),
	};
	wl_signal_emit_mutable(&pointer->events.swipe_begin, &wlr_event);
}

void handle_pointer_swipe_update(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_gesture *gevent =
		libinput_event_get_gesture_event(event);
	struct wlr_pointer_swipe_update_event wlr_event = {
		.pointer = pointer,
		.time_msec =
			usec_to_msec(libinput_event_gesture_get_time_usec(gevent)),
		.fingers = libinput_event_gesture_get_finger_count(gevent),
		.dx = libinput_event_gesture_get_dx(gevent),
		.dy = libinput_event_gesture_get_dy(gevent),
	};
	wl_signal_emit_mutable(&pointer->events.swipe_update, &wlr_event);
}

void handle_pointer_swipe_end(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_gesture *gevent =
		libinput_event_get_gesture_event(event);
	struct wlr_pointer_swipe_end_event wlr_event = {
		.pointer = pointer,
		.time_msec =
			usec_to_msec(libinput_event_gesture_get_time_usec(gevent)),
		.cancelled = libinput_event_gesture_get_cancelled(gevent),
	};
	wl_signal_emit_mutable(&pointer->events.swipe_end, &wlr_event);
}

void handle_pointer_pinch_begin(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_gesture *gevent =
		libinput_event_get_gesture_event(event);
	struct wlr_pointer_pinch_begin_event wlr_event = {
		.pointer = pointer,
		.time_msec =
			usec_to_msec(libinput_event_gesture_get_time_usec(gevent)),
		.fingers = libinput_event_gesture_get_finger_count(gevent),
	};
	wl_signal_emit_mutable(&pointer->events.pinch_begin, &wlr_event);
}

void handle_pointer_pinch_update(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_gesture *gevent =
		libinput_event_get_gesture_event(event);
	struct wlr_pointer_pinch_update_event wlr_event = {
		.pointer = pointer,
		.time_msec =
			usec_to_msec(libinput_event_gesture_get_time_usec(gevent)),
		.fingers = libinput_event_gesture_get_finger_count(gevent),
		.dx = libinput_event_gesture_get_dx(gevent),
		.dy = libinput_event_gesture_get_dy(gevent),
		.scale = libinput_event_gesture_get_scale(gevent),
		.rotation = libinput_event_gesture_get_angle_delta(gevent),
	};
	wl_signal_emit_mutable(&pointer->events.pinch_update, &wlr_event);
}

void handle_pointer_pinch_end(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_gesture *gevent =
		libinput_event_get_gesture_event(event);
	struct wlr_pointer_pinch_end_event wlr_event = {
		.pointer = pointer,
		.time_msec =
			usec_to_msec(libinput_event_gesture_get_time_usec(gevent)),
		.cancelled = libinput_event_gesture_get_cancelled(gevent),
	};
	wl_signal_emit_mutable(&pointer->events.pinch_end, &wlr_event);
}

void handle_pointer_hold_begin(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_gesture *gevent =
		libinput_event_get_gesture_event(event);
	struct wlr_pointer_hold_begin_event wlr_event = {
		.pointer = pointer,
		.time_msec =
			usec_to_msec(libinput_event_gesture_get_time_usec(gevent)),
		.fingers = libinput_event_gesture_get_finger_count(gevent),
	};
	wl_signal_emit_mutable(&pointer->events.hold_begin, &wlr_event);
}

void handle_pointer_hold_end(struct libinput_event *event,
		struct wlr_pointer *pointer) {
	struct libinput_event_gesture *gevent =
		libinput_event_get_gesture_event(event);
	struct wlr_pointer_hold_end_event wlr_event = {
		.pointer = pointer,
		.time_msec =
			usec_to_msec(libinput_event_gesture_get_time_usec(gevent)),
		.cancelled = libinput_event_gesture_get_cancelled(gevent),
	};
	wl_signal_emit_mutable(&pointer->events.hold_end, &wlr_event);
}
