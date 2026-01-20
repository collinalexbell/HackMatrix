#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/log.h>
#include "types/wlr_seat.h"

static void default_pointer_enter(struct wlr_seat_pointer_grab *grab,
		struct wlr_surface *surface, double sx, double sy) {
	wlr_seat_pointer_enter(grab->seat, surface, sx, sy);
}

static void default_pointer_clear_focus(struct wlr_seat_pointer_grab *grab) {
	wlr_seat_pointer_clear_focus(grab->seat);
}

static void default_pointer_motion(struct wlr_seat_pointer_grab *grab,
		uint32_t time, double sx, double sy) {
	wlr_seat_pointer_send_motion(grab->seat, time, sx, sy);
}

static uint32_t default_pointer_button(struct wlr_seat_pointer_grab *grab,
		uint32_t time, uint32_t button, enum wl_pointer_button_state state) {
	return wlr_seat_pointer_send_button(grab->seat, time, button, state);
}

static void default_pointer_axis(struct wlr_seat_pointer_grab *grab,
		uint32_t time, enum wl_pointer_axis orientation, double value,
		int32_t value_discrete, enum wl_pointer_axis_source source,
		enum wl_pointer_axis_relative_direction relative_direction) {
	wlr_seat_pointer_send_axis(grab->seat, time, orientation, value,
		value_discrete, source, relative_direction);
}

static void default_pointer_frame(struct wlr_seat_pointer_grab *grab) {
	wlr_seat_pointer_send_frame(grab->seat);
}

static void default_pointer_cancel(struct wlr_seat_pointer_grab *grab) {
	// cannot be cancelled
}

const struct wlr_pointer_grab_interface default_pointer_grab_impl = {
	.enter = default_pointer_enter,
	.clear_focus = default_pointer_clear_focus,
	.motion = default_pointer_motion,
	.button = default_pointer_button,
	.axis = default_pointer_axis,
	.frame = default_pointer_frame,
	.cancel = default_pointer_cancel,
};


static void pointer_send_frame(struct wl_resource *resource) {
	if (wl_resource_get_version(resource) >=
			WL_POINTER_FRAME_SINCE_VERSION) {
		wl_pointer_send_frame(resource);
	}
}

static const struct wl_pointer_interface pointer_impl;

struct wlr_seat_client *wlr_seat_client_from_pointer_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wl_pointer_interface,
		&pointer_impl));
	return wl_resource_get_user_data(resource);
}

static void pointer_cursor_surface_handle_commit(struct wlr_surface *surface) {
	pixman_region32_clear(&surface->input_region);
	if (wlr_surface_has_buffer(surface)) {
		wlr_surface_map(surface);
	}
}

static const struct wlr_surface_role pointer_cursor_surface_role = {
	.name = "wl_pointer-cursor",
	.no_object = true,
	.commit = pointer_cursor_surface_handle_commit,
};

static void pointer_set_cursor(struct wl_client *client,
		struct wl_resource *pointer_resource, uint32_t serial,
		struct wl_resource *surface_resource,
		int32_t hotspot_x, int32_t hotspot_y) {
	struct wlr_seat_client *seat_client =
		wlr_seat_client_from_pointer_resource(pointer_resource);
	if (seat_client == NULL) {
		return;
	}

	struct wlr_surface *surface = NULL;
	if (surface_resource != NULL) {
		surface = wlr_surface_from_resource(surface_resource);
		if (!wlr_surface_set_role(surface, &pointer_cursor_surface_role,
				surface_resource, WL_POINTER_ERROR_ROLE)) {
			return;
		}

		pointer_cursor_surface_handle_commit(surface);
	}

	struct wlr_seat_pointer_request_set_cursor_event event = {
		.seat_client = seat_client,
		.surface = surface,
		.serial = serial,
		.hotspot_x = hotspot_x,
		.hotspot_y = hotspot_y,
	};
	wl_signal_emit_mutable(&seat_client->seat->events.request_set_cursor, &event);
}

static void pointer_release(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wl_pointer_interface pointer_impl = {
	.set_cursor = pointer_set_cursor,
	.release = pointer_release,
};

static void pointer_handle_resource_destroy(struct wl_resource *resource) {
	seat_client_destroy_pointer(resource);
}


bool wlr_seat_pointer_surface_has_focus(struct wlr_seat *wlr_seat,
		struct wlr_surface *surface) {
	return surface == wlr_seat->pointer_state.focused_surface;
}

static void seat_pointer_handle_surface_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_seat_pointer_state *state =
		wl_container_of(listener, state, surface_destroy);
	wl_list_remove(&state->surface_destroy.link);
	wl_list_init(&state->surface_destroy.link);
	wlr_seat_pointer_clear_focus(state->seat);
}

void seat_client_send_pointer_leave_raw(struct wlr_seat_client *seat_client,
		struct wlr_surface *surface) {
	uint32_t serial = wlr_seat_client_next_serial(seat_client);
	struct wl_resource *resource;
	wl_resource_for_each(resource, &seat_client->pointers) {
		if (wlr_seat_client_from_pointer_resource(resource) == NULL) {
			continue;
		}

		wl_pointer_send_leave(resource, serial, surface->resource);
		pointer_send_frame(resource);
	}
}

void wlr_seat_pointer_enter(struct wlr_seat *wlr_seat,
		struct wlr_surface *surface, double sx, double sy) {
	if (wlr_seat->pointer_state.focused_surface == surface) {
		// this surface already got an enter notify
		return;
	}

	struct wlr_seat_client *client = NULL;
	if (surface) {
		struct wl_client *wl_client = wl_resource_get_client(surface->resource);
		client = wlr_seat_client_for_wl_client(wlr_seat, wl_client);
	}

	struct wlr_seat_client *focused_client =
		wlr_seat->pointer_state.focused_client;
	struct wlr_surface *focused_surface =
		wlr_seat->pointer_state.focused_surface;

	// leave the previously entered surface
	if (focused_client != NULL && focused_surface != NULL) {
		seat_client_send_pointer_leave_raw(focused_client, focused_surface);
	}

	// enter the current surface
	if (client != NULL && surface != NULL) {
		uint32_t serial = wlr_seat_client_next_serial(client);
		struct wl_resource *resource;
		wl_resource_for_each(resource, &client->pointers) {
			if (wlr_seat_client_from_pointer_resource(resource) == NULL) {
				continue;
			}

			wl_pointer_send_enter(resource, serial, surface->resource,
				wl_fixed_from_double(sx), wl_fixed_from_double(sy));
			pointer_send_frame(resource);
		}
	}

	// reinitialize the focus destroy events
	wl_list_remove(&wlr_seat->pointer_state.surface_destroy.link);
	wl_list_init(&wlr_seat->pointer_state.surface_destroy.link);
	if (surface != NULL) {
		wl_signal_add(&surface->events.destroy,
			&wlr_seat->pointer_state.surface_destroy);
		wlr_seat->pointer_state.surface_destroy.notify =
			seat_pointer_handle_surface_destroy;
	}

	wlr_seat->pointer_state.focused_client = client;
	wlr_seat->pointer_state.focused_surface = surface;
	if (surface != NULL) {
		wlr_seat_pointer_warp(wlr_seat, sx, sy);
	} else {
		wlr_seat_pointer_warp(wlr_seat, NAN, NAN);
	}

	struct wlr_seat_pointer_focus_change_event event = {
		.seat = wlr_seat,
		.new_surface = surface,
		.old_surface = focused_surface,
		.sx = sx,
		.sy = sy,
	};
	wl_signal_emit_mutable(&wlr_seat->pointer_state.events.focus_change, &event);
}

void wlr_seat_pointer_clear_focus(struct wlr_seat *wlr_seat) {
	wlr_seat_pointer_enter(wlr_seat, NULL, 0, 0);
}

void wlr_seat_pointer_warp(struct wlr_seat *wlr_seat, double sx, double sy) {
	wlr_seat->pointer_state.sx = sx;
	wlr_seat->pointer_state.sy = sy;
}

void wlr_seat_pointer_send_motion(struct wlr_seat *wlr_seat, uint32_t time,
		double sx, double sy) {
	struct wlr_seat_client *client = wlr_seat->pointer_state.focused_client;
	if (client == NULL) {
		return;
	}

	// Ensure we don't send duplicate motion events. Instead of comparing with an
	// epsilon, chop off some precision by converting to a `wl_fixed_t` first,
	// since that is what a client receives.
	wl_fixed_t sx_fixed = wl_fixed_from_double(sx);
	wl_fixed_t sy_fixed = wl_fixed_from_double(sy);
	if (wl_fixed_from_double(wlr_seat->pointer_state.sx) != sx_fixed ||
			wl_fixed_from_double(wlr_seat->pointer_state.sy) != sy_fixed) {
		struct wl_resource *resource;
		wl_resource_for_each(resource, &client->pointers) {
			if (wlr_seat_client_from_pointer_resource(resource) == NULL) {
				continue;
			}

			wl_pointer_send_motion(resource, time, sx_fixed, sy_fixed);
		}
	}

	wlr_seat_pointer_warp(wlr_seat, sx, sy);
}

uint32_t wlr_seat_pointer_send_button(struct wlr_seat *wlr_seat, uint32_t time,
		uint32_t button, enum wl_pointer_button_state state) {
	struct wlr_seat_client *client = wlr_seat->pointer_state.focused_client;
	if (client == NULL) {
		return 0;
	}

	uint32_t serial = wlr_seat_client_next_serial(client);
	struct wl_resource *resource;
	wl_resource_for_each(resource, &client->pointers) {
		if (wlr_seat_client_from_pointer_resource(resource) == NULL) {
			continue;
		}

		wl_pointer_send_button(resource, serial, time, button, state);
	}
	return serial;
}

static bool should_reset_value120_accumulators(int32_t current, int32_t last) {
	if (last == 0) {
		return true;
	}

	return (current < 0 && last > 0) || (current > 0 && last < 0);
}

static void update_value120_accumulators(struct wlr_seat_client *client,
		enum wl_pointer_axis orientation,
		double value, int32_t value_discrete,
		double *low_res_value, int32_t *low_res_value_discrete) {
	if (value_discrete == 0) {
		// Continuous scrolling has no effect on accumulators
		return;
	}

	int32_t *acc_discrete = &client->value120.acc_discrete[orientation];
	int32_t *last_discrete = &client->value120.last_discrete[orientation];
	double *acc_axis = &client->value120.acc_axis[orientation];

	if (should_reset_value120_accumulators(value_discrete, *last_discrete)) {
		*acc_discrete = 0;
		*acc_axis = 0;
	}
	*acc_discrete += value_discrete;
	*last_discrete = value_discrete;
	*acc_axis += value;

	// Compute low resolution event values for older clients and reset
	// the accumulators if needed
	*low_res_value_discrete = *acc_discrete / WLR_POINTER_AXIS_DISCRETE_STEP;
	if (*low_res_value_discrete == 0) {
		*low_res_value = 0;
	} else {
		*acc_discrete -= *low_res_value_discrete * WLR_POINTER_AXIS_DISCRETE_STEP;
		*low_res_value = *acc_axis;
		*acc_axis = 0;
	}
}

void wlr_seat_pointer_send_axis(struct wlr_seat *wlr_seat, uint32_t time,
		enum wl_pointer_axis orientation, double value,
		int32_t value_discrete, enum wl_pointer_axis_source source,
		enum wl_pointer_axis_relative_direction relative_direction) {
	struct wlr_seat_client *client = wlr_seat->pointer_state.focused_client;
	if (client == NULL) {
		return;
	}

	bool send_source = false;
	if (wlr_seat->pointer_state.sent_axis_source) {
		assert(wlr_seat->pointer_state.cached_axis_source == source);
	} else {
		wlr_seat->pointer_state.sent_axis_source = true;
		wlr_seat->pointer_state.cached_axis_source = source;
		send_source = true;
	}

	double low_res_value = 0.0;
	int32_t low_res_value_discrete = 0;
	update_value120_accumulators(client, orientation, value, value_discrete,
		&low_res_value, &low_res_value_discrete);

	struct wl_resource *resource;
	wl_resource_for_each(resource, &client->pointers) {
		if (wlr_seat_client_from_pointer_resource(resource) == NULL) {
			continue;
		}

		uint32_t version = wl_resource_get_version(resource);

		if (version < WL_POINTER_AXIS_VALUE120_SINCE_VERSION &&
				value_discrete != 0 && low_res_value_discrete == 0) {
			// The client doesn't support high resolution discrete scrolling
			// and we haven't accumulated enough wheel clicks for a single
			// low resolution event. Don't send anything.
			continue;
		}

		if (send_source && version >= WL_POINTER_AXIS_SOURCE_SINCE_VERSION) {
			wl_pointer_send_axis_source(resource, source);
		}
		if (value) {
			if (version >= WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION) {
				wl_pointer_send_axis_relative_direction(resource,
					orientation, relative_direction);
			}
			if (value_discrete) {
				if (version >= WL_POINTER_AXIS_VALUE120_SINCE_VERSION) {
					// High resolution discrete scrolling
					wl_pointer_send_axis_value120(resource, orientation,
						value_discrete);
					wl_pointer_send_axis(resource, time, orientation,
						wl_fixed_from_double(value));
				} else {
					// Low resolution discrete scrolling
					if (version >= WL_POINTER_AXIS_DISCRETE_SINCE_VERSION) {
						wl_pointer_send_axis_discrete(resource, orientation,
							low_res_value_discrete);
					}
					wl_pointer_send_axis(resource, time, orientation,
						wl_fixed_from_double(low_res_value));
				}
			} else {
				// Continuous scrolling
				wl_pointer_send_axis(resource, time, orientation,
					wl_fixed_from_double(value));
			}
		} else if (version >= WL_POINTER_AXIS_STOP_SINCE_VERSION) {
			wl_pointer_send_axis_stop(resource, time, orientation);
		}
	}
}

void wlr_seat_pointer_send_frame(struct wlr_seat *wlr_seat) {
	struct wlr_seat_client *client = wlr_seat->pointer_state.focused_client;
	if (client == NULL) {
		return;
	}

	wlr_seat->pointer_state.sent_axis_source = false;

	struct wl_resource *resource;
	wl_resource_for_each(resource, &client->pointers) {
		if (wlr_seat_client_from_pointer_resource(resource) == NULL) {
			continue;
		}

		pointer_send_frame(resource);
	}
}

void wlr_seat_pointer_start_grab(struct wlr_seat *wlr_seat,
		struct wlr_seat_pointer_grab *grab) {
	assert(wlr_seat);
	grab->seat = wlr_seat;
	wlr_seat->pointer_state.grab = grab;

	wl_signal_emit_mutable(&wlr_seat->events.pointer_grab_begin, grab);
}

void wlr_seat_pointer_end_grab(struct wlr_seat *wlr_seat) {
	struct wlr_seat_pointer_grab *grab = wlr_seat->pointer_state.grab;
	if (grab != wlr_seat->pointer_state.default_grab) {
		wlr_seat->pointer_state.grab = wlr_seat->pointer_state.default_grab;
		wl_signal_emit_mutable(&wlr_seat->events.pointer_grab_end, grab);
		if (grab->interface->cancel) {
			grab->interface->cancel(grab);
		}
	}
}

// Switching focus means the new surface doesn't know about the currently
// pressed buttons. This function allows to reset them.
static void reset_buttons(struct wlr_seat *wlr_seat) {
	wlr_seat->pointer_state.button_count = 0;
}

void wlr_seat_pointer_notify_enter(struct wlr_seat *wlr_seat,
		struct wlr_surface *surface, double sx, double sy) {
	// NULL surfaces are prohibited in the grab-compatible API. Use
	// wlr_seat_pointer_notify_clear_focus() instead.
	assert(surface);
	struct wlr_seat_pointer_grab *grab = wlr_seat->pointer_state.grab;
	struct wlr_surface *focused_surface = wlr_seat->pointer_state.focused_surface;

	grab->interface->enter(grab, surface, sx, sy);

	if (focused_surface != wlr_seat->pointer_state.focused_surface) {
		reset_buttons(wlr_seat);
	}
}

void wlr_seat_pointer_notify_clear_focus(struct wlr_seat *wlr_seat) {
	struct wlr_seat_pointer_grab *grab = wlr_seat->pointer_state.grab;
	struct wlr_surface *focused_surface = wlr_seat->pointer_state.focused_surface;

	grab->interface->clear_focus(grab);

	if (focused_surface != wlr_seat->pointer_state.focused_surface) {
		reset_buttons(wlr_seat);
	}
}

void wlr_seat_pointer_notify_motion(struct wlr_seat *wlr_seat, uint32_t time,
		double sx, double sy) {
	struct wlr_seat_pointer_grab *grab = wlr_seat->pointer_state.grab;
	grab->interface->motion(grab, time, sx, sy);
}

uint32_t wlr_seat_pointer_notify_button(struct wlr_seat *wlr_seat,
		uint32_t time, uint32_t button, enum wl_pointer_button_state state) {
	struct wlr_seat_pointer_state* pointer_state = &wlr_seat->pointer_state;

	if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
		if (pointer_state->button_count == 0) {
			pointer_state->grab_button = button;
			pointer_state->grab_time = time;
		}
		for (size_t i = 0; i < pointer_state->button_count; i++) {
			struct wlr_seat_pointer_button *pointer_button = &pointer_state->buttons[i];
			if (pointer_button->button == button) {
				++pointer_button->n_pressed;
				return 0;
			}
		}
		if (pointer_state->button_count == WLR_POINTER_BUTTONS_CAP) {
			return 0;
		}
		pointer_state->buttons[pointer_state->button_count++] = (struct wlr_seat_pointer_button){
			.button = button,
			.n_pressed = 1,
		};
	} else {
		bool found = false;
		for (size_t i = 0; i < pointer_state->button_count; i++) {
			struct wlr_seat_pointer_button *pointer_button = &pointer_state->buttons[i];
			if (pointer_button->button == button) {
				if (--pointer_button->n_pressed > 0) {
					return 0;
				}
				*pointer_button = pointer_state->buttons[--pointer_state->button_count];
				found = true;
				break;
			}
		}
		if (!found) {
			return 0;
		}
	}

	struct wlr_seat_pointer_grab *grab = pointer_state->grab;
	uint32_t serial = grab->interface->button(grab, time, button, state);

	if (serial && pointer_state->button_count == 1 &&
			state == WL_POINTER_BUTTON_STATE_PRESSED) {
		pointer_state->grab_serial = serial;
	}

	return serial;
}

void wlr_seat_pointer_notify_axis(struct wlr_seat *wlr_seat, uint32_t time,
		enum wl_pointer_axis orientation, double value,
		int32_t value_discrete, enum wl_pointer_axis_source source,
		enum wl_pointer_axis_relative_direction relative_direction) {
	struct wlr_seat_pointer_grab *grab = wlr_seat->pointer_state.grab;
	grab->interface->axis(grab, time, orientation, value, value_discrete,
		source, relative_direction);
}

void wlr_seat_pointer_notify_frame(struct wlr_seat *wlr_seat) {
	struct wlr_seat_pointer_grab *grab = wlr_seat->pointer_state.grab;
	if (grab->interface->frame) {
		grab->interface->frame(grab);
	}
}

bool wlr_seat_pointer_has_grab(struct wlr_seat *seat) {
	return seat->pointer_state.grab->interface != &default_pointer_grab_impl;
}

void seat_client_create_pointer(struct wlr_seat_client *seat_client,
		uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(seat_client->client,
		&wl_pointer_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(seat_client->client);
		return;
	}
	wl_resource_set_implementation(resource, &pointer_impl, seat_client,
		&pointer_handle_resource_destroy);
	wl_list_insert(&seat_client->pointers, wl_resource_get_link(resource));

	if ((seat_client->seat->capabilities & WL_SEAT_CAPABILITY_POINTER) == 0) {
		wl_resource_set_user_data(resource, NULL);
		return;
	}

	struct wlr_seat_client *focused_client =
		seat_client->seat->pointer_state.focused_client;
	struct wlr_surface *focused_surface =
		seat_client->seat->pointer_state.focused_surface;

	// Send an enter event if there is a focused client/surface stored
	if (focused_client == seat_client && focused_surface != NULL) {
		double sx = seat_client->seat->pointer_state.sx;
		double sy = seat_client->seat->pointer_state.sy;

		uint32_t serial = wlr_seat_client_next_serial(focused_client);
		struct wl_resource *resource;
		wl_resource_for_each(resource, &focused_client->pointers) {
			if (wl_resource_get_id(resource) == id) {
				if (wlr_seat_client_from_pointer_resource(resource) == NULL) {
					continue;
				}

				wl_pointer_send_enter(resource, serial, focused_surface->resource,
					wl_fixed_from_double(sx), wl_fixed_from_double(sy));
				pointer_send_frame(resource);
			}
		}
	}
}

void seat_client_create_inert_pointer(struct wl_client *client,
		uint32_t version, uint32_t id) {
	struct wl_resource *resource =
		wl_resource_create(client, &wl_pointer_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &pointer_impl, NULL, NULL);
}

void seat_client_destroy_pointer(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
	wl_list_init(wl_resource_get_link(resource));
	wl_resource_set_user_data(resource, NULL);
}

bool wlr_seat_validate_pointer_grab_serial(struct wlr_seat *seat,
		struct wlr_surface *origin, uint32_t serial) {
	if (seat->pointer_state.button_count != 1 ||
			seat->pointer_state.grab_serial != serial) {
		wlr_log(WLR_DEBUG, "Pointer grab serial validation failed: "
			"button_count=%zu grab_serial=%"PRIu32" (got %"PRIu32")",
			seat->pointer_state.button_count,
			seat->pointer_state.grab_serial, serial);
		return false;
	}

	if (origin != NULL && seat->pointer_state.focused_surface != origin) {
		wlr_log(WLR_DEBUG, "Pointer grab serial validation failed: "
			"invalid origin surface");
		return false;
	}

	return true;
}
