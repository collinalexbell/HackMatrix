
#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/interfaces/wlr_tablet_pad.h>
#include <wlr/interfaces/wlr_tablet_tool.h>
#include <wlr/util/log.h>

#include "backend/wayland.h"
#include "util/time.h"

#include "tablet-v2-client-protocol.h"

struct tablet_tool {
	/* static */
	struct wlr_wl_seat *seat;

	/* semi-static */
	struct wlr_wl_output *output;
	double pre_x, pre_y;

	/* per frame */
	double x, y;

	double pressure;
	double distance;
	double tilt_x, tilt_y;
	double rotation;
	double slider;
	double wheel_delta;

	bool is_in;
	bool is_out;

	bool is_up;
	bool is_down;
};

struct tablet_pad_ring {
	struct wl_list link; // tablet_pad_group.rings
	/* static */
	struct zwp_tablet_pad_ring_v2 *ring;
	struct tablet_pad_group *group;
	size_t index;

	/* per frame */
	enum wlr_tablet_pad_ring_source source;
	double angle;
	bool stopped;
};

struct tablet_pad_strip {
	struct wl_list link; // tablet_pad_group.strips
	struct zwp_tablet_pad_strip_v2 *strip;
	struct tablet_pad_group *group;
	size_t index;

	enum wlr_tablet_pad_strip_source source;
	double position;
	bool stopped;
};

struct tablet_pad_group {
	struct zwp_tablet_pad_group_v2 *pad_group;
	struct wlr_tablet_pad *pad;
	unsigned int mode;

	struct wlr_tablet_pad_group group;

	struct wl_list rings; // tablet_pad_ring.link
	struct wl_list strips; // tablet_pad_strips.link
};

static void handle_tablet_pad_ring_source(void *data,
		struct zwp_tablet_pad_ring_v2 *zwp_tablet_pad_ring_v2,
		uint32_t source) {
	struct tablet_pad_ring *ring = data;
	ring->source = source;
}

static void handle_tablet_pad_ring_angle(void *data,
		struct zwp_tablet_pad_ring_v2 *zwp_tablet_pad_ring_v2,
		wl_fixed_t degrees) {
	struct tablet_pad_ring *ring = data;
	ring->angle = wl_fixed_to_double(degrees);
}

static void handle_tablet_pad_ring_stop(void *data,
		struct zwp_tablet_pad_ring_v2 *zwp_tablet_pad_ring_v2) {
	struct tablet_pad_ring *ring = data;
	ring->stopped = true;
}

static void handle_tablet_pad_ring_frame(void *data,
		struct zwp_tablet_pad_ring_v2 *zwp_tablet_pad_ring_v2,
		uint32_t time) {
	struct tablet_pad_ring *ring = data;

	struct wlr_tablet_pad_ring_event evt = {
		.time_msec = time,
		.source = ring->source,
		.ring = ring->index,
		.position = ring->angle,
		.mode = ring->group->mode,
	};

	if (ring->angle >= 0) {
		wl_signal_emit_mutable(&ring->group->pad->events.ring, &evt);
	}
	if (ring->stopped) {
		evt.position = -1;
		wl_signal_emit_mutable(&ring->group->pad->events.ring, &evt);
	}

	ring->angle = -1;
	ring->stopped = false;
	ring->source = 0;
}

static const struct zwp_tablet_pad_ring_v2_listener tablet_pad_ring_listener = {
	.source = handle_tablet_pad_ring_source,
	.angle = handle_tablet_pad_ring_angle,
	.stop = handle_tablet_pad_ring_stop,
	.frame = handle_tablet_pad_ring_frame,
};

static void handle_tablet_pad_strip_source(void *data,
		struct zwp_tablet_pad_strip_v2 *zwp_tablet_pad_strip_v2,
		uint32_t source) {
	struct tablet_pad_strip *strip = data;
	strip->source = source;
}

static void handle_tablet_pad_strip_position(void *data,
		struct zwp_tablet_pad_strip_v2 *zwp_tablet_pad_strip_v2,
		uint32_t position) {
	struct tablet_pad_strip *strip = data;
	strip->position = (double) position / 65536.0;
}

static void handle_tablet_pad_strip_stop(void *data,
		struct zwp_tablet_pad_strip_v2 *zwp_tablet_pad_strip_v2) {
	struct tablet_pad_strip *strip = data;
	strip->stopped = true;
}

static void handle_tablet_pad_strip_frame(void *data,
		struct zwp_tablet_pad_strip_v2 *zwp_tablet_pad_strip_v2,
		uint32_t time) {
	struct tablet_pad_strip *strip = data;

	struct wlr_tablet_pad_strip_event evt = {
		.time_msec = time,
		.source = strip->source,
		.strip = strip->index,
		.position = strip->position,
		.mode = strip->group->mode,
	};

	if (strip->position >= 0) {
		wl_signal_emit_mutable(&strip->group->pad->events.strip, &evt);
	}
	if (strip->stopped) {
		evt.position = -1;
		wl_signal_emit_mutable(&strip->group->pad->events.strip, &evt);
	}

	strip->position = -1;
	strip->stopped = false;
	strip->source = 0;
}

static const struct zwp_tablet_pad_strip_v2_listener tablet_pad_strip_listener = {
	.source = handle_tablet_pad_strip_source,
	.position = handle_tablet_pad_strip_position,
	.stop = handle_tablet_pad_strip_stop,
	.frame = handle_tablet_pad_strip_frame,
};

static void handle_tablet_pad_group_buttons(void *data,
		struct zwp_tablet_pad_group_v2 *pad_group,
		struct wl_array *buttons) {
	struct tablet_pad_group *group = data;

	free(group->group.buttons);
	group->group.buttons = calloc(1, buttons->size);
	if (!group->group.buttons) {
		// FIXME: Add actual error handling
		return;
	}

	group->group.button_count = buttons->size / sizeof(int);
	memcpy(group->group.buttons, buttons->data, buttons->size);
}

static void handle_tablet_pad_group_modes(void *data,
		struct zwp_tablet_pad_group_v2 *pad_group, uint32_t modes) {
	struct tablet_pad_group *group = data;

	group->group.mode_count = modes;
}

static void handle_tablet_pad_group_ring(void *data,
		struct zwp_tablet_pad_group_v2 *pad_group,
		struct zwp_tablet_pad_ring_v2 *ring) {
	struct tablet_pad_group *group = data;
	struct tablet_pad_ring *tablet_ring = calloc(1, sizeof(*tablet_ring));
	if (!tablet_ring) {
		zwp_tablet_pad_ring_v2_destroy(ring);
		return;
	}
	tablet_ring->index = group->pad->ring_count++;
	tablet_ring->group = group;
	zwp_tablet_pad_ring_v2_add_listener(ring, &tablet_pad_ring_listener,
		tablet_ring);

	group->group.rings = realloc(group->group.rings,
		++group->group.ring_count * sizeof(unsigned int));
	group->group.rings[group->group.ring_count - 1] =
		tablet_ring->index;
}

static void handle_tablet_pad_group_strip(void *data,
		struct zwp_tablet_pad_group_v2 *pad_group,
		struct zwp_tablet_pad_strip_v2 *strip) {
	struct tablet_pad_group *group = data;
	struct tablet_pad_strip *tablet_strip = calloc(1, sizeof(*tablet_strip));
	if (!tablet_strip) {
		zwp_tablet_pad_strip_v2_destroy(strip);
		return;
	}
	tablet_strip->index = group->pad->strip_count++;
	tablet_strip->group = group;
	zwp_tablet_pad_strip_v2_add_listener(strip, &tablet_pad_strip_listener,
		tablet_strip);

	group->group.strips = realloc(group->group.strips,
		++group->group.strip_count * sizeof(unsigned int));
	group->group.strips[group->group.strip_count - 1] =
		tablet_strip->index;
}

static void handle_tablet_pad_group_done(void *data,
		struct zwp_tablet_pad_group_v2 *pad_group) {
	/* Empty for now */
}

static void handle_tablet_pad_group_mode_switch(void *data,
		struct zwp_tablet_pad_group_v2 *pad_group,
		uint32_t time, uint32_t serial, uint32_t mode) {
	struct tablet_pad_group *group = data;
	group->mode = mode;
}

static void destroy_tablet_pad_group(struct tablet_pad_group *group) {
	/* No need to remove the link on strips rings as long as we do *not*
	 * wl_list_remove on the wl_groups ring/strip attributes here */
	struct tablet_pad_ring *ring, *tmp_ring;
	wl_list_for_each_safe(ring, tmp_ring, &group->rings, link) {
		zwp_tablet_pad_ring_v2_destroy(ring->ring);
		free(ring);
	}

	struct tablet_pad_strip *strip, *tmp_strip;
	wl_list_for_each_safe(strip, tmp_strip, &group->strips, link) {
		zwp_tablet_pad_strip_v2_destroy(strip->strip);
		free(strip);
	}

	zwp_tablet_pad_group_v2_destroy(group->pad_group);

	free(group->group.buttons);
	free(group->group.strips);
	free(group->group.rings);
	wl_list_remove(&group->group.link);

	free(group);
}

static const struct zwp_tablet_pad_group_v2_listener tablet_pad_group_listener = {
	.buttons = handle_tablet_pad_group_buttons,
	.modes = handle_tablet_pad_group_modes,
	.ring = handle_tablet_pad_group_ring,
	.strip = handle_tablet_pad_group_strip,
	.done = handle_tablet_pad_group_done,
	.mode_switch = handle_tablet_pad_group_mode_switch,
};

static void handle_tablet_pad_group(void *data,
		struct zwp_tablet_pad_v2 *zwp_tablet_pad,
		struct zwp_tablet_pad_group_v2 *pad_group) {
	struct wlr_wl_seat *seat = data;
	struct wlr_tablet_pad *pad = &seat->wlr_tablet_pad;

	struct tablet_pad_group *group = calloc(1, sizeof(*group));
	if (!group) {
		wlr_log_errno(WLR_ERROR, "failed to allocate tablet_pad_group");
		zwp_tablet_pad_group_v2_destroy(pad_group);
		return;
	}
	group->pad_group = pad_group;
	group->pad = pad;

	wl_list_init(&group->rings);
	wl_list_init(&group->strips);

	zwp_tablet_pad_group_v2_add_listener(pad_group,
		&tablet_pad_group_listener, group);

	wl_list_insert(&pad->groups, &group->group.link);
}

static void handle_tablet_pad_path(void *data,
		struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2, const char *path) {
	struct wlr_wl_seat *seat = data;
	struct wlr_tablet_pad *tablet_pad = &seat->wlr_tablet_pad;

	char **dst = wl_array_add(&tablet_pad->paths, sizeof(char *));
	*dst = strdup(path);
}

static void handle_tablet_pad_buttons(void *data,
		struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2, uint32_t buttons) {
	struct wlr_wl_seat *seat = data;
	struct wlr_tablet_pad *tablet_pad = &seat->wlr_tablet_pad;

	tablet_pad->button_count = buttons;
}

static void handle_tablet_pad_button(void *data,
		struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2,
		uint32_t time, uint32_t button, uint32_t state) {
	struct wlr_wl_seat *seat = data;
	struct wlr_tablet_pad_button_event evt = {
		.time_msec = time,
		.button = button,
		.state = state,
		.mode = 0,
		.group = 0,
	};

	wl_signal_emit_mutable(&seat->wlr_tablet_pad.events.button, &evt);
}

static void handle_tablet_pad_done(void *data,
		struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2) {
	struct wlr_wl_seat *seat = data;
	wl_signal_emit_mutable(&seat->backend->backend.events.new_input,
		&seat->wlr_tablet_pad.base);
}

static void handle_tablet_pad_enter(void *data,
		struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2,
		uint32_t serial, struct zwp_tablet_v2 *tablet_p,
		struct wl_surface *surface) {
	struct wlr_wl_seat *seat = data;
	assert(seat->zwp_tablet_v2 == tablet_p);

	wl_signal_emit_mutable(&seat->wlr_tablet_pad.events.attach_tablet,
		&seat->wlr_tablet_tool);
}

static void handle_tablet_pad_leave(void *data,
		struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2,
		uint32_t serial, struct wl_surface *surface) {
	/* Empty. Probably staying that way, unless we want to create/destroy
	 * tablet on enter/leave events (ehh) */
}

static void handle_tablet_pad_removed(void *data,
		struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2) {
	struct wlr_wl_seat *seat = data;

	struct wlr_tablet_pad *tablet_pad = &seat->wlr_tablet_pad;
	struct tablet_pad_group *group, *it;
	wl_list_for_each_safe(group, it, &tablet_pad->groups, group.link) {
		destroy_tablet_pad_group(group);
	}

	wlr_tablet_pad_finish(tablet_pad);
	zwp_tablet_pad_v2_destroy(seat->zwp_tablet_pad_v2);
	seat->zwp_tablet_pad_v2 = NULL;
}

static const struct zwp_tablet_pad_v2_listener tablet_pad_listener = {
	.group = handle_tablet_pad_group,
	.path = handle_tablet_pad_path,
	.buttons = handle_tablet_pad_buttons,
	.button = handle_tablet_pad_button,
	.done = handle_tablet_pad_done,
	.enter = handle_tablet_pad_enter,
	.leave = handle_tablet_pad_leave,
	.removed = handle_tablet_pad_removed,
};

const struct wlr_tablet_pad_impl wl_tablet_pad_impl = {
	.name = "wl-tablet-pad",
};

static void handle_pad_added(void *data,
		struct zwp_tablet_seat_v2 *zwp_tablet_seat_v2,
		struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2) {
	struct wlr_wl_seat *seat = data;
	if (seat->zwp_tablet_pad_v2 != NULL) {
		wlr_log(WLR_ERROR, "zwp_tablet_pad_v2 is already present");
		zwp_tablet_pad_v2_destroy(zwp_tablet_pad_v2);
		return;
	}

	seat->zwp_tablet_pad_v2 = zwp_tablet_pad_v2;
	zwp_tablet_pad_v2_add_listener(zwp_tablet_pad_v2, &tablet_pad_listener,
		seat);

	wlr_tablet_pad_init(&seat->wlr_tablet_pad, &wl_tablet_pad_impl,
		"wlr_tablet_v2");
}

static void handle_tablet_tool_done(void *data,
		struct zwp_tablet_tool_v2 *id) {
	/* empty */
}

static enum wlr_tablet_tool_type tablet_type_to_wlr_type(
		enum zwp_tablet_tool_v2_type type) {
	switch (type) {
	case ZWP_TABLET_TOOL_V2_TYPE_PEN:
		return WLR_TABLET_TOOL_TYPE_PEN;
	case ZWP_TABLET_TOOL_V2_TYPE_ERASER:
		return WLR_TABLET_TOOL_TYPE_ERASER;
	case ZWP_TABLET_TOOL_V2_TYPE_BRUSH:
		return WLR_TABLET_TOOL_TYPE_BRUSH;
	case ZWP_TABLET_TOOL_V2_TYPE_PENCIL:
		return WLR_TABLET_TOOL_TYPE_PENCIL;
	case ZWP_TABLET_TOOL_V2_TYPE_AIRBRUSH:
		return WLR_TABLET_TOOL_TYPE_AIRBRUSH;
	case ZWP_TABLET_TOOL_V2_TYPE_MOUSE:
		return WLR_TABLET_TOOL_TYPE_MOUSE;
	case ZWP_TABLET_TOOL_V2_TYPE_LENS:
		return WLR_TABLET_TOOL_TYPE_LENS;
	case ZWP_TABLET_TOOL_V2_TYPE_FINGER:
		// unused, see:
		// https://gitlab.freedesktop.org/wayland/wayland-protocols/-/issues/18
		abort();
	}
	abort(); // unreachable
}

static void handle_tablet_tool_type(void *data,
		struct zwp_tablet_tool_v2 *id, uint32_t tool_type) {
	struct tablet_tool *tool = data;
	struct wlr_tablet_tool *wlr_tool = &tool->seat->wlr_tablet_tool;
	wlr_tool->type = tablet_type_to_wlr_type(tool_type);
}

static void handle_tablet_tool_serial(void *data,
		struct zwp_tablet_tool_v2 *id, uint32_t high, uint32_t low) {
	struct tablet_tool *tool = data;
	struct wlr_tablet_tool *wlr_tool = &tool->seat->wlr_tablet_tool;
	wlr_tool->hardware_serial = ((uint64_t) high) << 32 | (uint64_t) low;
}

static void handle_tablet_tool_id_wacom(void *data,
		struct zwp_tablet_tool_v2 *id, uint32_t high, uint32_t low) {
	struct tablet_tool *tool = data;
	struct wlr_tablet_tool *wlr_tool = &tool->seat->wlr_tablet_tool;
	wlr_tool->hardware_wacom = ((uint64_t) high) << 32 | (uint64_t) low;
}

static void handle_tablet_tool_capability(void *data,
		struct zwp_tablet_tool_v2 *id, uint32_t capability) {
	struct tablet_tool *tool = data;
	struct wlr_tablet_tool *wlr_tool = &tool->seat->wlr_tablet_tool;

	/* One event is sent for each capability */
	switch (capability) {
	case ZWP_TABLET_TOOL_V2_CAPABILITY_TILT:
		wlr_tool->tilt = true;
		break;
	case ZWP_TABLET_TOOL_V2_CAPABILITY_PRESSURE:
		wlr_tool->pressure = true;
		break;
	case ZWP_TABLET_TOOL_V2_CAPABILITY_DISTANCE:
		wlr_tool->distance = true;
		break;
	case ZWP_TABLET_TOOL_V2_CAPABILITY_ROTATION:
		wlr_tool->rotation = true;
		break;
	case ZWP_TABLET_TOOL_V2_CAPABILITY_SLIDER:
		wlr_tool->slider = true;
		break;
	case ZWP_TABLET_TOOL_V2_CAPABILITY_WHEEL:
		wlr_tool->wheel = true;
		break;
	}
}

static void handle_tablet_tool_proximity_in(void *data,
		struct zwp_tablet_tool_v2 *id, uint32_t serial,
		struct zwp_tablet_v2 *tablet_id, struct wl_surface *surface) {
	struct tablet_tool *tool = data;
	assert(tablet_id == tool->seat->zwp_tablet_v2);

	struct wlr_wl_output *output = get_wl_output_from_surface(tool->seat->backend, surface);
	if (output == NULL) {
		return;
	}

	tool->is_in = true;
	tool->output = output;
}

static void handle_tablet_tool_proximity_out(void *data,
		struct zwp_tablet_tool_v2 *id) {
	struct tablet_tool *tool = data;
	tool->is_out = true;
	tool->output = NULL;
}

static void handle_tablet_tool_down(void *data, struct zwp_tablet_tool_v2 *id,
		unsigned int serial) {
	struct tablet_tool *tool = data;
	tool->is_down = true;
}

static void handle_tablet_tool_up(void *data, struct zwp_tablet_tool_v2 *id) {
	struct tablet_tool *tool = data;
	tool->is_up = true;
}

static void handle_tablet_tool_motion(void *data, struct zwp_tablet_tool_v2 *id,
		wl_fixed_t x, wl_fixed_t y) {
	struct tablet_tool *tool = data;
	struct wlr_wl_output *output = tool->output;
	assert(output);

	tool->x = wl_fixed_to_double(x) / output->wlr_output.width;
	tool->y = wl_fixed_to_double(y) / output->wlr_output.height;
}

static void handle_tablet_tool_pressure(void *data,
		struct zwp_tablet_tool_v2 *id, uint32_t pressure) {
	struct tablet_tool *tool = data;
	tool->pressure = (double) pressure / 65535.0;
}

static void handle_tablet_tool_distance(void *data,
		struct zwp_tablet_tool_v2 *id, uint32_t distance) {
	struct tablet_tool *tool = data;
	tool->distance = (double) distance / 65535.0;
}

static void handle_tablet_tool_tilt(void *data, struct zwp_tablet_tool_v2 *id,
		wl_fixed_t x, wl_fixed_t y) {
	struct tablet_tool *tool = data;
	tool->tilt_x = wl_fixed_to_double(x);
	tool->tilt_y = wl_fixed_to_double(y);
}

static void handle_tablet_tool_rotation(void *data,
		struct zwp_tablet_tool_v2 *id, wl_fixed_t rotation) {
	struct tablet_tool *tool = data;
	tool->rotation = wl_fixed_to_double(rotation);
}

static void handle_tablet_tool_slider(void *data, struct zwp_tablet_tool_v2 *id,
		int slider) {
	struct tablet_tool *tool = data;
	tool->slider = (double) slider / 65535.0;;
}

// TODO: This looks wrong :/
static void handle_tablet_tool_wheel(void *data, struct zwp_tablet_tool_v2 *id,
		wl_fixed_t degree, int clicks) {
	struct tablet_tool *tool = data;
	tool->wheel_delta = wl_fixed_to_double(degree);
}

static void handle_tablet_tool_button(void *data,
		struct zwp_tablet_tool_v2 *id,
		uint32_t serial, uint32_t button, uint32_t state) {
	struct tablet_tool *tool = data;
	struct wlr_wl_seat *seat = tool->seat;
	struct wlr_tablet *tablet = &seat->wlr_tablet;

	struct wlr_tablet_tool_button_event evt = {
		.tablet = tablet,
		.tool = &seat->wlr_tablet_tool,
		.time_msec = get_current_time_msec(),
		.button = button,
		.state = state == ZWP_TABLET_TOOL_V2_BUTTON_STATE_RELEASED ?
			WLR_BUTTON_RELEASED : WLR_BUTTON_PRESSED,
	};

	wl_signal_emit_mutable(&tablet->events.button, &evt);
}

static void clear_tablet_tool_values(struct tablet_tool *tool) {
	tool->is_out = tool->is_in = false;
	tool->is_up = tool->is_down = false;
	tool->x = tool->y = NAN;
	tool->pressure = NAN;
	tool->distance = NAN;
	tool->tilt_x = tool->tilt_y = NAN;
	tool->rotation = NAN;
	tool->slider = NAN;
	tool->wheel_delta = NAN;
}

static void handle_tablet_tool_frame(void *data,
		struct zwp_tablet_tool_v2 *id,
		uint32_t time) {
	struct tablet_tool *tool = data;
	struct wlr_wl_seat *seat = tool->seat;

	if (tool->is_out && tool->is_in) {
		/* we got a tablet tool coming in and out of proximity before
		 * we could process it. Just ignore anything it did */
		goto clear_values;
	}
	struct wlr_tablet *tablet = &seat->wlr_tablet;

	if (tool->is_in) {
		struct wlr_tablet_tool_proximity_event evt = {
			.tablet = tablet,
			.tool = &seat->wlr_tablet_tool,
			.time_msec = time,
			.x = tool->x,
			.y = tool->y,
			.state = WLR_TABLET_TOOL_PROXIMITY_IN,
		};

		wl_signal_emit_mutable(&tablet->events.proximity, &evt);
	}

	{
		struct wlr_tablet_tool_axis_event evt = {
			.tablet = tablet,
			.tool = &seat->wlr_tablet_tool,
			.time_msec = time,
			.updated_axes = 0,
		};

		if (!isnan(tool->x) && !tool->is_in) {
			evt.updated_axes |= WLR_TABLET_TOOL_AXIS_X;
			evt.x = tool->x;
		}

		if (!isnan(tool->y) && !tool->is_in) {
			evt.updated_axes |= WLR_TABLET_TOOL_AXIS_Y;
			evt.y = tool->y;
		}

		if (!isnan(tool->pressure)) {
			evt.updated_axes |= WLR_TABLET_TOOL_AXIS_PRESSURE;
			evt.pressure = tool->pressure;
		}

		if (!isnan(tool->distance)) {
			evt.updated_axes |= WLR_TABLET_TOOL_AXIS_DISTANCE;
			evt.distance = tool->distance;
		}

		if (!isnan(tool->tilt_x)) {
			evt.updated_axes |= WLR_TABLET_TOOL_AXIS_TILT_X;
			evt.tilt_x = tool->tilt_x;
		}

		if (!isnan(tool->tilt_y)) {
			evt.updated_axes |= WLR_TABLET_TOOL_AXIS_TILT_Y;
			evt.tilt_y = tool->tilt_y;
		}

		if (!isnan(tool->rotation)) {
			evt.updated_axes |= WLR_TABLET_TOOL_AXIS_ROTATION;
			evt.rotation = tool->rotation;
		}

		if (!isnan(tool->slider)) {
			evt.updated_axes |= WLR_TABLET_TOOL_AXIS_SLIDER;
			evt.slider = tool->slider;
		}

		if (!isnan(tool->wheel_delta)) {
			evt.updated_axes |= WLR_TABLET_TOOL_AXIS_WHEEL;
			evt.wheel_delta = tool->wheel_delta;
		}

		if (evt.updated_axes) {
			wl_signal_emit_mutable(&tablet->events.axis, &evt);
		}
	}

	/* This will always send down then up if we got both.
	 * Maybe we should send them right away, in case we get up then both in
	 * series?
	 * Downside: Here we have the frame time, if we sent right away, we
	 * need to generate the time */
	if (tool->is_down) {
		struct wlr_tablet_tool_tip_event evt = {
			.tablet = tablet,
			.tool = &seat->wlr_tablet_tool,
			.time_msec = time,
			.x = tool->x,
			.y = tool->y,
			.state = WLR_TABLET_TOOL_TIP_DOWN,
		};

		wl_signal_emit_mutable(&tablet->events.tip, &evt);
	}

	if (tool->is_up) {
		struct wlr_tablet_tool_tip_event evt = {
			.tablet = tablet,
			.tool = &seat->wlr_tablet_tool,
			.time_msec = time,
			.x = tool->x,
			.y = tool->y,
			.state = WLR_TABLET_TOOL_TIP_UP,
		};

		wl_signal_emit_mutable(&tablet->events.tip, &evt);
	}

	if (tool->is_out) {
		struct wlr_tablet_tool_proximity_event evt = {
			.tablet = tablet,
			.tool = &seat->wlr_tablet_tool,
			.time_msec = time,
			.x = tool->x,
			.y = tool->y,
			.state = WLR_TABLET_TOOL_PROXIMITY_OUT,
		};

		wl_signal_emit_mutable(&tablet->events.proximity, &evt);
	}

clear_values:
	clear_tablet_tool_values(tool);
}

static void handle_tablet_tool_removed(void *data,
		struct zwp_tablet_tool_v2 *id) {
	struct tablet_tool *tool = data;
	struct wlr_wl_seat *seat = tool->seat;

	zwp_tablet_tool_v2_destroy(seat->zwp_tablet_tool_v2);
	seat->zwp_tablet_tool_v2 = NULL;

	free(tool);
}

static const struct zwp_tablet_tool_v2_listener tablet_tool_listener = {
	.removed = handle_tablet_tool_removed,
	.done = handle_tablet_tool_done,
	.type = handle_tablet_tool_type,
	.hardware_serial = handle_tablet_tool_serial,
	.hardware_id_wacom = handle_tablet_tool_id_wacom,
	.capability = handle_tablet_tool_capability,

	.proximity_in = handle_tablet_tool_proximity_in,
	.proximity_out = handle_tablet_tool_proximity_out,
	.down = handle_tablet_tool_down,
	.up = handle_tablet_tool_up,

	.motion = handle_tablet_tool_motion,
	.pressure = handle_tablet_tool_pressure,
	.distance = handle_tablet_tool_distance,
	.tilt = handle_tablet_tool_tilt,
	.rotation = handle_tablet_tool_rotation,
	.slider = handle_tablet_tool_slider,
	.wheel = handle_tablet_tool_wheel,
	.button = handle_tablet_tool_button,
	.frame = handle_tablet_tool_frame,
};

static void handle_tool_added(void *data,
		struct zwp_tablet_seat_v2 *zwp_tablet_seat_v2,
		struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2) {
	struct wlr_wl_seat *seat = data;
	if (seat->zwp_tablet_tool_v2 != NULL) {
		wlr_log(WLR_ERROR, "zwp_tablet_tool_v2 already present");
		zwp_tablet_tool_v2_destroy(zwp_tablet_tool_v2);
		return;
	}

	wl_signal_init(&seat->wlr_tablet_tool.events.destroy);

	struct tablet_tool *tool = calloc(1, sizeof(*tool));
	if (tool == NULL) {
		wlr_log_errno(WLR_ERROR, "failed to allocate tablet_tool");
		zwp_tablet_tool_v2_destroy(zwp_tablet_tool_v2);
		return;
	}

	tool->seat = seat;
	clear_tablet_tool_values(tool);

	seat->zwp_tablet_tool_v2 = zwp_tablet_tool_v2;
	zwp_tablet_tool_v2_add_listener(seat->zwp_tablet_tool_v2, &tablet_tool_listener,
		tool);
}

static void handle_tablet_name(void *data, struct zwp_tablet_v2 *zwp_tablet_v2,
		const char *name) {
	struct wlr_wl_seat *seat = data;
	struct wlr_tablet *tablet = &seat->wlr_tablet;

	free(tablet->base.name);
	tablet->base.name = strdup(name);
}

static void handle_tablet_id(void *data, struct zwp_tablet_v2 *zwp_tablet_v2,
		uint32_t vid, uint32_t pid) {
	struct wlr_wl_seat *seat = data;
	struct wlr_tablet *tablet = &seat->wlr_tablet;

	tablet->usb_vendor_id = vid;
	tablet->usb_product_id = pid;
}

static void handle_tablet_path(void *data, struct zwp_tablet_v2 *zwp_tablet_v2,
		const char *path) {
	struct wlr_wl_seat *seat = data;
	struct wlr_tablet *tablet = &seat->wlr_tablet;

	char **dst = wl_array_add(&tablet->paths, sizeof(char *));
	*dst = strdup(path);
}

static void handle_tablet_done(void *data, struct zwp_tablet_v2 *zwp_tablet_v2) {
	struct wlr_wl_seat *seat = data;

	wl_signal_emit_mutable(&seat->backend->backend.events.new_input,
		&seat->wlr_tablet.base);
}

static void handle_tablet_removed(void *data,
		struct zwp_tablet_v2 *zwp_tablet_v2) {
	struct wlr_wl_seat *seat = data;

	wlr_tablet_finish(&seat->wlr_tablet);
	zwp_tablet_v2_destroy(seat->zwp_tablet_v2);
	seat->zwp_tablet_v2 = NULL;
}

static const struct zwp_tablet_v2_listener tablet_listener = {
	.name = handle_tablet_name,
	.id = handle_tablet_id,
	.path = handle_tablet_path,
	.done = handle_tablet_done,
	.removed = handle_tablet_removed,
};

const struct wlr_tablet_impl wl_tablet_impl = {
	.name = "wl-tablet-tool",
};

static void handle_tab_added(void *data,
		struct zwp_tablet_seat_v2 *zwp_tablet_seat_v2,
		struct zwp_tablet_v2 *zwp_tablet_v2) {
	struct wlr_wl_seat *seat = data;
	if (seat->zwp_tablet_v2 != NULL) {
		wlr_log(WLR_ERROR, "zwp_tablet_v2 already present");
		zwp_tablet_v2_destroy(zwp_tablet_v2);
		return;
	}

	seat->zwp_tablet_v2 = zwp_tablet_v2;
	zwp_tablet_v2_add_listener(zwp_tablet_v2, &tablet_listener, seat);

	wlr_tablet_init(&seat->wlr_tablet, &wl_tablet_impl, "wlr_tablet_v2");
}

static const struct zwp_tablet_seat_v2_listener tablet_seat_listener = {
	.tablet_added = handle_tab_added,
	.tool_added = handle_tool_added,
	.pad_added = handle_pad_added,
};

void init_seat_tablet(struct wlr_wl_seat *seat) {
	struct zwp_tablet_manager_v2 *manager = seat->backend->tablet_manager;
	assert(manager);

	/**
	 * TODO: multi tablet support
	 * The wlr_wl_seat should support multiple tablet_v2 devices, but for
	 * the sake of simplicity, it supports only one device of each.
	 * If this is a feature you want/need, please open an issue on the wlroots
	 * tracker here https://gitlab.freedesktop.org/wlroots/wlroots/-/issues
	 */

	seat->zwp_tablet_seat_v2 =
		zwp_tablet_manager_v2_get_tablet_seat(manager, seat->wl_seat);
	if (seat->zwp_tablet_seat_v2 == NULL) {
		wlr_log(WLR_ERROR, "failed to get zwp_tablet_manager_v2 from seat '%s'",
			seat->name);
		return;
	}

	zwp_tablet_seat_v2_add_listener(seat->zwp_tablet_seat_v2,
		&tablet_seat_listener, seat);
}

void finish_seat_tablet(struct wlr_wl_seat *seat) {
	if (seat->zwp_tablet_v2 != NULL) {
		wlr_tablet_finish(&seat->wlr_tablet);
		zwp_tablet_v2_destroy(seat->zwp_tablet_v2);
	}

	if (seat->zwp_tablet_tool_v2 != NULL) {
		struct tablet_tool *tool =
			zwp_tablet_tool_v2_get_user_data(seat->zwp_tablet_tool_v2);
		free(tool);

		zwp_tablet_tool_v2_destroy(seat->zwp_tablet_tool_v2);
	}

	if (seat->zwp_tablet_pad_v2 != NULL) {
		struct wlr_tablet_pad *tablet_pad = &seat->wlr_tablet_pad;
		struct tablet_pad_group *group, *it;
		wl_list_for_each_safe(group, it, &tablet_pad->groups, group.link) {
			destroy_tablet_pad_group(group);
		}

		wlr_tablet_pad_finish(tablet_pad);
		zwp_tablet_pad_v2_destroy(seat->zwp_tablet_pad_v2);
	}

	zwp_tablet_seat_v2_destroy(seat->zwp_tablet_seat_v2);
	seat->zwp_tablet_seat_v2 = NULL;
}
