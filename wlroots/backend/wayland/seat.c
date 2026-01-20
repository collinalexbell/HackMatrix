#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <wayland-client-protocol.h>

#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/interfaces/wlr_touch.h>
#include <wlr/interfaces/wlr_tablet_tool.h>
#include <wlr/interfaces/wlr_tablet_pad.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/util/log.h>

#include "backend/wayland.h"
#include "util/time.h"

static void keyboard_handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t format, int32_t fd, uint32_t size) {
	close(fd);
}

static void keyboard_handle_enter(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface, struct wl_array *keys) {
	struct wlr_keyboard *keyboard = data;

	int64_t time_msec = get_current_time_msec();

	uint32_t *keycode_ptr;
	wl_array_for_each(keycode_ptr, keys) {
		struct wlr_keyboard_key_event event = {
			.keycode = *keycode_ptr,
			.state = WL_KEYBOARD_KEY_STATE_PRESSED,
			.time_msec = time_msec,
			.update_state = false,
		};
		wlr_keyboard_notify_key(keyboard, &event);
	}
}

static void keyboard_handle_leave(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, struct wl_surface *surface) {
	struct wlr_keyboard *keyboard = data;

	int64_t time_msec = get_current_time_msec();
	while (keyboard->num_keycodes > 0) {
		struct wlr_keyboard_key_event event = {
			.keycode = keyboard->keycodes[keyboard->num_keycodes - 1],
			.state = WL_KEYBOARD_KEY_STATE_RELEASED,
			.time_msec = time_msec,
			.update_state = false,
		};
		wlr_keyboard_notify_key(keyboard, &event);
	}
}

static void keyboard_handle_key(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t time, uint32_t key, uint32_t state) {
	struct wlr_keyboard *keyboard = data;

	struct wlr_keyboard_key_event wlr_event = {
		.keycode = key,
		.state = state,
		.time_msec = time,
		.update_state = false,
	};
	wlr_keyboard_notify_key(keyboard, &wlr_event);
}

static void keyboard_handle_modifiers(void *data, struct wl_keyboard *wl_keyboard,
		uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched,
		uint32_t mods_locked, uint32_t group) {
	struct wlr_keyboard *keyboard = data;
	wlr_keyboard_notify_modifiers(keyboard, mods_depressed, mods_latched,
		mods_locked, group);
}

static void keyboard_handle_repeat_info(void *data,
		struct wl_keyboard *wl_keyboard, int32_t rate, int32_t delay) {
	// This space is intentionally left blank
}

static const struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_handle_keymap,
	.enter = keyboard_handle_enter,
	.leave = keyboard_handle_leave,
	.key = keyboard_handle_key,
	.modifiers = keyboard_handle_modifiers,
	.repeat_info = keyboard_handle_repeat_info
};

static const struct wlr_keyboard_impl keyboard_impl = {
	.name = "wl-keyboard",
};

void init_seat_keyboard(struct wlr_wl_seat *seat) {
	assert(seat->wl_keyboard);

	char name[128] = {0};
	snprintf(name, sizeof(name), "wayland-keyboard-%s", seat->name);

	wlr_keyboard_init(&seat->wlr_keyboard, &keyboard_impl, name);
	wl_keyboard_add_listener(seat->wl_keyboard, &keyboard_listener,
		&seat->wlr_keyboard);

	wl_signal_emit_mutable(&seat->backend->backend.events.new_input,
		&seat->wlr_keyboard.base);
}

static void touch_coordinates_to_absolute(struct wlr_wl_seat *seat,
		wl_fixed_t x, wl_fixed_t y, double *sx, double *sy) {
	/**
	 * TODO: multi-output touch support
	 * Although the wayland backend supports multi-output pointers, the support
	 * for multi-output touch has been left on the side for simplicity reasons.
	 * If this is a feature you want/need, please open an issue on the wlroots
	 * tracker here https://gitlab.freedesktop.org/wlroots/wlroots/-/issues
	 */
	struct wlr_wl_output *output, *tmp;
	wl_list_for_each_safe(output, tmp, &seat->backend->outputs, link) {
		*sx = wl_fixed_to_double(x) / output->wlr_output.width;
		*sy = wl_fixed_to_double(y) / output->wlr_output.height;
		return; // Choose the first output in the list
	}

	*sx = *sy = 0;
}

static void touch_handle_down(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, struct wl_surface *surface,
		int32_t id, wl_fixed_t x, wl_fixed_t y) {
	struct wlr_wl_seat *seat = data;
	struct wlr_touch *touch = &seat->wlr_touch;

	struct wlr_wl_touch_points *points = &seat->touch_points;
	assert(points->len != sizeof(points->ids) / sizeof(points->ids[0]));
	points->ids[points->len++] = id;

	struct wlr_touch_down_event event = {
		.touch = touch,
		.time_msec = time,
		.touch_id = id,
	};
	touch_coordinates_to_absolute(seat, x, y, &event.x, &event.y);
	wl_signal_emit_mutable(&touch->events.down, &event);
}

static bool remove_touch_point(struct wlr_wl_touch_points *points, int32_t id) {
	size_t i = 0;
	for (; i < points->len; i++) {
		if (points->ids[i] == id) {
			size_t remaining = points->len - i - 1;
			memmove(&points->ids[i], &points->ids[i + 1], remaining * sizeof(id));
			points->len--;
			return true;
		}
	}
	return false;
}

static void touch_handle_up(void *data, struct wl_touch *wl_touch,
		uint32_t serial, uint32_t time, int32_t id) {
	struct wlr_wl_seat *seat = data;
	struct wlr_touch *touch = &seat->wlr_touch;

	remove_touch_point(&seat->touch_points, id);

	struct wlr_touch_up_event event = {
		.touch = touch,
		.time_msec = time,
		.touch_id = id,
	};
	wl_signal_emit_mutable(&touch->events.up, &event);
}

static void touch_handle_motion(void *data, struct wl_touch *wl_touch,
		uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y) {
	struct wlr_wl_seat *seat = data;
	struct wlr_touch *touch = &seat->wlr_touch;

	struct wlr_touch_motion_event event = {
		.touch = touch,
		.time_msec = time,
		.touch_id = id,
	};

	touch_coordinates_to_absolute(seat, x, y, &event.x, &event.y);
	wl_signal_emit_mutable(&touch->events.motion, &event);
}

static void touch_handle_frame(void *data, struct wl_touch *wl_touch) {
	struct wlr_wl_seat *seat = data;
	wl_signal_emit_mutable(&seat->wlr_touch.events.frame, NULL);
}

static void touch_handle_cancel(void *data, struct wl_touch *wl_touch) {
	struct wlr_wl_seat *seat = data;
	struct wlr_touch *touch = &seat->wlr_touch;

	// wayland's cancel event applies to all active touch points
	for (size_t i = 0; i < seat->touch_points.len; i++) {
		struct wlr_touch_cancel_event event = {
			.touch = touch,
			.time_msec = 0,
			.touch_id = seat->touch_points.ids[i],
		};
		wl_signal_emit_mutable(&touch->events.cancel, &event);
	}
	seat->touch_points.len = 0;
}

static void touch_handle_shape(void *data, struct wl_touch *wl_touch,
		int32_t id, wl_fixed_t major, wl_fixed_t minor) {
	// no-op
}

static void touch_handle_orientation(void *data, struct wl_touch *wl_touch,
		int32_t id, wl_fixed_t orientation) {
	// no-op
}

static const struct wl_touch_listener touch_listener = {
	.down = touch_handle_down,
	.up = touch_handle_up,
	.motion = touch_handle_motion,
	.frame = touch_handle_frame,
	.cancel = touch_handle_cancel,
	.shape = touch_handle_shape,
	.orientation = touch_handle_orientation,
};

static const struct wlr_touch_impl touch_impl = {
	.name = "wl-touch",
};

void init_seat_touch(struct wlr_wl_seat *seat) {
	assert(seat->wl_touch);

	char name[128] = {0};
	snprintf(name, sizeof(name), "wayland-touch-%s", seat->name);

	wlr_touch_init(&seat->wlr_touch, &touch_impl, name);

	struct wlr_wl_output *output;
	wl_list_for_each(output, &seat->backend->outputs, link) {
		/* Multi-output touch not supproted */
		seat->wlr_touch.output_name = strdup(output->wlr_output.name);
		break;
	}

	wl_touch_add_listener(seat->wl_touch, &touch_listener, seat);
	wl_signal_emit_mutable(&seat->backend->backend.events.new_input,
		&seat->wlr_touch.base);
}

static const struct wl_seat_listener seat_listener;

bool create_wl_seat(struct wl_seat *wl_seat, struct wlr_wl_backend *wl,
		uint32_t global_name) {
	struct wlr_wl_seat *seat = calloc(1, sizeof(*seat));
	if (!seat) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return false;
	}
	seat->wl_seat = wl_seat;
	seat->backend = wl;
	seat->global_name = global_name;
	wl_list_insert(&wl->seats, &seat->link);
	wl_seat_add_listener(wl_seat, &seat_listener, seat);
	return true;
}

void destroy_wl_seat(struct wlr_wl_seat *seat) {
	if (seat->wl_touch) {
		wl_touch_release(seat->wl_touch);
		wlr_touch_finish(&seat->wlr_touch);
	}
	if (seat->wl_pointer) {
		finish_seat_pointer(seat);
	}
	if (seat->wl_keyboard) {
		wl_keyboard_release(seat->wl_keyboard);

		if (seat->backend->started) {
			wlr_keyboard_finish(&seat->wlr_keyboard);
		}
	}
	if (seat->zwp_tablet_seat_v2) {
		finish_seat_tablet(seat);
	}

	free(seat->name);
	assert(seat->wl_seat);
	wl_seat_destroy(seat->wl_seat);

	wl_list_remove(&seat->link);
	free(seat);
}

bool wlr_input_device_is_wl(struct wlr_input_device *dev) {
	switch (dev->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		return wlr_keyboard_from_input_device(dev)->impl == &keyboard_impl;
	case WLR_INPUT_DEVICE_POINTER:
		return wlr_pointer_from_input_device(dev)->impl == &wl_pointer_impl;
	case WLR_INPUT_DEVICE_TOUCH:
		return wlr_touch_from_input_device(dev)->impl == &touch_impl;
	case WLR_INPUT_DEVICE_TABLET:
		return wlr_tablet_from_input_device(dev)-> impl == &wl_tablet_impl;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		return wlr_tablet_pad_from_input_device(dev)->impl == &wl_tablet_pad_impl;
	default:
		return false;
	}
}

static void seat_handle_capabilities(void *data, struct wl_seat *wl_seat,
		enum wl_seat_capability caps) {
	struct wlr_wl_seat *seat = data;
	struct wlr_wl_backend *backend = seat->backend;

	if ((caps & WL_SEAT_CAPABILITY_POINTER) && seat->wl_pointer == NULL) {
		wlr_log(WLR_DEBUG, "seat '%s' offering pointer", seat->name);

		seat->wl_pointer = wl_seat_get_pointer(wl_seat);
		init_seat_pointer(seat);
	}
	if (!(caps & WL_SEAT_CAPABILITY_POINTER) && seat->wl_pointer != NULL) {
		wlr_log(WLR_DEBUG, "seat '%s' dropping pointer", seat->name);
		finish_seat_pointer(seat);
	}

	if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && seat->wl_keyboard == NULL) {
		wlr_log(WLR_DEBUG, "seat '%s' offering keyboard", seat->name);

		struct wl_keyboard *wl_keyboard = wl_seat_get_keyboard(wl_seat);
		seat->wl_keyboard = wl_keyboard;

		if (backend->started) {
			init_seat_keyboard(seat);
		}
	}
	if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && seat->wl_keyboard != NULL) {
		wlr_log(WLR_DEBUG, "seat '%s' dropping keyboard", seat->name);

		wl_keyboard_release(seat->wl_keyboard);
		wlr_keyboard_finish(&seat->wlr_keyboard);

		seat->wl_keyboard = NULL;
	}

	if ((caps & WL_SEAT_CAPABILITY_TOUCH) && seat->wl_touch == NULL) {
		wlr_log(WLR_DEBUG, "seat '%s' offering touch", seat->name);

		seat->wl_touch = wl_seat_get_touch(wl_seat);
		if (backend->started) {
			init_seat_touch(seat);
		}
	}
	if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && seat->wl_touch != NULL) {
		wlr_log(WLR_DEBUG, "seat '%s' dropping touch", seat->name);

		wl_touch_release(seat->wl_touch);
		wlr_touch_finish(&seat->wlr_touch);
		seat->wl_touch = NULL;
	}
}

static void seat_handle_name(void *data, struct wl_seat *wl_seat,
		const char *name) {
	struct wlr_wl_seat *seat = data;
	free(seat->name);
	seat->name = strdup(name);
}

static const struct wl_seat_listener seat_listener = {
	.capabilities = seat_handle_capabilities,
	.name = seat_handle_name,
};
