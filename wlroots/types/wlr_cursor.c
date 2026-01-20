#include <assert.h>
#include <drm_fourcc.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "types/wlr_buffer.h"
#include "types/wlr_output.h"

struct wlr_cursor_device {
	struct wlr_cursor *cursor;
	struct wlr_input_device *device;
	struct wl_list link;
	struct wlr_output *mapped_output;
	struct wlr_box mapped_box; // empty if unset

	struct wl_listener motion;
	struct wl_listener motion_absolute;
	struct wl_listener button;
	struct wl_listener axis;
	struct wl_listener frame;
	struct wl_listener swipe_begin;
	struct wl_listener swipe_update;
	struct wl_listener swipe_end;
	struct wl_listener pinch_begin;
	struct wl_listener pinch_update;
	struct wl_listener pinch_end;
	struct wl_listener hold_begin;
	struct wl_listener hold_end;

	struct wl_listener touch_down;
	struct wl_listener touch_up;
	struct wl_listener touch_motion;
	struct wl_listener touch_cancel;
	struct wl_listener touch_frame;

	struct wl_listener tablet_tool_axis;
	struct wl_listener tablet_tool_proximity;
	struct wl_listener tablet_tool_tip;
	struct wl_listener tablet_tool_button;

	struct wl_listener destroy;
};

struct wlr_cursor_output_cursor {
	struct wlr_cursor *cursor;
	struct wlr_output_cursor *output_cursor;
	struct wl_list link;

	struct wl_listener layout_output_destroy;

	// only when using a surface as the cursor image
	struct wl_listener output_commit;

	// only when using an XCursor as the cursor image
	struct wlr_xcursor *xcursor;
	size_t xcursor_index;
	struct wl_event_source *xcursor_timer;
};

struct wlr_cursor_state {
	struct wlr_cursor cursor;

	struct wl_list devices; // wlr_cursor_device.link
	struct wl_list output_cursors; // wlr_cursor_output_cursor.link
	struct wlr_output_layout *layout;
	struct wlr_output *mapped_output;
	struct wlr_box mapped_box; // empty if unset

	struct wl_listener layout_add;
	struct wl_listener layout_change;
	struct wl_listener layout_destroy;

	// only when using a buffer as the cursor image
	struct wlr_buffer *buffer;
	struct {
		int32_t x, y;
	} buffer_hotspot;
	float buffer_scale;

	// only when using a surface as the cursor image
	struct wlr_surface *surface;
	struct {
		int32_t x, y;
	} surface_hotspot;
	struct wl_listener surface_commit;
	struct wl_listener surface_destroy;

	// only when using an XCursor as the cursor image
	struct wlr_xcursor_manager *xcursor_manager;
	char *xcursor_name;
};

struct wlr_cursor *wlr_cursor_create(void) {
	struct wlr_cursor_state *state = calloc(1, sizeof(*state));
	if (!state) {
		wlr_log(WLR_ERROR, "Failed to allocate wlr_cursor_state");
		return NULL;
	}
	struct wlr_cursor *cur = &state->cursor;

	cur->state = state;

	wl_list_init(&cur->state->devices);
	wl_list_init(&cur->state->output_cursors);

	// pointer signals
	wl_signal_init(&cur->events.motion);
	wl_signal_init(&cur->events.motion_absolute);
	wl_signal_init(&cur->events.button);
	wl_signal_init(&cur->events.axis);
	wl_signal_init(&cur->events.frame);
	wl_signal_init(&cur->events.swipe_begin);
	wl_signal_init(&cur->events.swipe_update);
	wl_signal_init(&cur->events.swipe_end);
	wl_signal_init(&cur->events.pinch_begin);
	wl_signal_init(&cur->events.pinch_update);
	wl_signal_init(&cur->events.pinch_end);
	wl_signal_init(&cur->events.hold_begin);
	wl_signal_init(&cur->events.hold_end);

	// touch signals
	wl_signal_init(&cur->events.touch_up);
	wl_signal_init(&cur->events.touch_down);
	wl_signal_init(&cur->events.touch_motion);
	wl_signal_init(&cur->events.touch_cancel);
	wl_signal_init(&cur->events.touch_frame);

	// tablet tool signals
	wl_signal_init(&cur->events.tablet_tool_tip);
	wl_signal_init(&cur->events.tablet_tool_axis);
	wl_signal_init(&cur->events.tablet_tool_button);
	wl_signal_init(&cur->events.tablet_tool_proximity);

	wl_list_init(&cur->state->surface_destroy.link);
	wl_list_init(&cur->state->surface_commit.link);

	cur->x = 100;
	cur->y = 100;

	return cur;
}

static void cursor_output_cursor_reset_image(struct wlr_cursor_output_cursor *output_cursor);

static void output_cursor_destroy(struct wlr_cursor_output_cursor *output_cursor) {
	cursor_output_cursor_reset_image(output_cursor);
	wl_list_remove(&output_cursor->layout_output_destroy.link);
	wl_list_remove(&output_cursor->link);
	wl_list_remove(&output_cursor->output_commit.link);
	wlr_output_cursor_destroy(output_cursor->output_cursor);
	free(output_cursor);
}

static void cursor_detach_output_layout(struct wlr_cursor *cur) {
	if (!cur->state->layout) {
		return;
	}

	struct wlr_cursor_output_cursor *output_cursor, *tmp;
	wl_list_for_each_safe(output_cursor, tmp, &cur->state->output_cursors,
			link) {
		output_cursor_destroy(output_cursor);
	}

	wl_list_remove(&cur->state->layout_destroy.link);
	wl_list_remove(&cur->state->layout_change.link);
	wl_list_remove(&cur->state->layout_add.link);

	cur->state->layout = NULL;
}

static void cursor_device_destroy(struct wlr_cursor_device *c_device) {
	struct wlr_input_device *dev = c_device->device;
	switch (dev->type) {
	case WLR_INPUT_DEVICE_POINTER:
		wl_list_remove(&c_device->motion.link);
		wl_list_remove(&c_device->motion_absolute.link);
		wl_list_remove(&c_device->button.link);
		wl_list_remove(&c_device->axis.link);
		wl_list_remove(&c_device->frame.link);
		wl_list_remove(&c_device->swipe_begin.link);
		wl_list_remove(&c_device->swipe_update.link);
		wl_list_remove(&c_device->swipe_end.link);
		wl_list_remove(&c_device->pinch_begin.link);
		wl_list_remove(&c_device->pinch_update.link);
		wl_list_remove(&c_device->pinch_end.link);
		wl_list_remove(&c_device->hold_begin.link);
		wl_list_remove(&c_device->hold_end.link);
		break;
	case WLR_INPUT_DEVICE_TOUCH:
		wl_list_remove(&c_device->touch_down.link);
		wl_list_remove(&c_device->touch_up.link);
		wl_list_remove(&c_device->touch_motion.link);
		wl_list_remove(&c_device->touch_cancel.link);
		wl_list_remove(&c_device->touch_frame.link);
		break;
	case WLR_INPUT_DEVICE_TABLET:
		wl_list_remove(&c_device->tablet_tool_axis.link);
		wl_list_remove(&c_device->tablet_tool_proximity.link);
		wl_list_remove(&c_device->tablet_tool_tip.link);
		wl_list_remove(&c_device->tablet_tool_button.link);
		break;
	default:
		abort(); // unreachable
	}

	wl_list_remove(&c_device->link);
	wl_list_remove(&c_device->destroy.link);
	free(c_device);
}

static void cursor_reset_image(struct wlr_cursor *cur) {
	wlr_buffer_unlock(cur->state->buffer);
	cur->state->buffer = NULL;

	if (cur->state->surface != NULL) {
		struct wlr_cursor_output_cursor *output_cursor;
		wl_list_for_each(output_cursor, &cur->state->output_cursors, link) {
			wlr_surface_send_leave(cur->state->surface,
				output_cursor->output_cursor->output);
		}
	}

	wl_list_remove(&cur->state->surface_destroy.link);
	wl_list_remove(&cur->state->surface_commit.link);
	wl_list_init(&cur->state->surface_destroy.link);
	wl_list_init(&cur->state->surface_commit.link);
	cur->state->surface = NULL;

	cur->state->xcursor_manager = NULL;
	free(cur->state->xcursor_name);
	cur->state->xcursor_name = NULL;
}

void wlr_cursor_destroy(struct wlr_cursor *cur) {
	// pointer signals
	assert(wl_list_empty(&cur->events.motion.listener_list));
	assert(wl_list_empty(&cur->events.motion_absolute.listener_list));
	assert(wl_list_empty(&cur->events.button.listener_list));
	assert(wl_list_empty(&cur->events.axis.listener_list));
	assert(wl_list_empty(&cur->events.frame.listener_list));
	assert(wl_list_empty(&cur->events.swipe_begin.listener_list));
	assert(wl_list_empty(&cur->events.swipe_update.listener_list));
	assert(wl_list_empty(&cur->events.swipe_end.listener_list));
	assert(wl_list_empty(&cur->events.pinch_begin.listener_list));
	assert(wl_list_empty(&cur->events.pinch_update.listener_list));
	assert(wl_list_empty(&cur->events.pinch_end.listener_list));
	assert(wl_list_empty(&cur->events.hold_begin.listener_list));
	assert(wl_list_empty(&cur->events.hold_end.listener_list));

	// touch signals
	assert(wl_list_empty(&cur->events.touch_up.listener_list));
	assert(wl_list_empty(&cur->events.touch_down.listener_list));
	assert(wl_list_empty(&cur->events.touch_motion.listener_list));
	assert(wl_list_empty(&cur->events.touch_cancel.listener_list));
	assert(wl_list_empty(&cur->events.touch_frame.listener_list));

	// tablet tool signals
	assert(wl_list_empty(&cur->events.tablet_tool_tip.listener_list));
	assert(wl_list_empty(&cur->events.tablet_tool_axis.listener_list));
	assert(wl_list_empty(&cur->events.tablet_tool_button.listener_list));
	assert(wl_list_empty(&cur->events.tablet_tool_proximity.listener_list));

	cursor_reset_image(cur);
	cursor_detach_output_layout(cur);

	struct wlr_cursor_device *device, *device_tmp = NULL;
	wl_list_for_each_safe(device, device_tmp, &cur->state->devices, link) {
		cursor_device_destroy(device);
	}

	free(cur->state);
}

static struct wlr_cursor_device *get_cursor_device(struct wlr_cursor *cur,
		struct wlr_input_device *device) {
	struct wlr_cursor_device *c_device, *ret = NULL;
	wl_list_for_each(c_device, &cur->state->devices, link) {
		if (c_device->device == device) {
			ret = c_device;
			break;
		}
	}

	return ret;
}

static void output_cursor_move(struct wlr_cursor_output_cursor *output_cursor) {
	struct wlr_cursor *cur = output_cursor->cursor;

	double output_x = cur->x, output_y = cur->y;
	wlr_output_layout_output_coords(cur->state->layout,
		output_cursor->output_cursor->output, &output_x, &output_y);
	wlr_output_cursor_move(output_cursor->output_cursor,
		output_x, output_y);
}

static void cursor_warp_unchecked(struct wlr_cursor *cur,
		double lx, double ly) {
	assert(cur->state->layout);
	if (!isfinite(lx) || !isfinite(ly)) {
		assert(false);
		return;
	}

	cur->x = lx;
	cur->y = ly;

	struct wlr_cursor_output_cursor *output_cursor;
	wl_list_for_each(output_cursor, &cur->state->output_cursors, link) {
		output_cursor_move(output_cursor);
	}
}

/**
 * Get the most specific mapping box for the device in this order:
 *
 * 1. device geometry mapping
 * 2. device output mapping
 * 3. cursor geometry mapping
 * 4. cursor output mapping
 *
 * Absolute movement for touch and pen devices will be relative to this box and
 * pointer movement will be constrained to this box.
 *
 * If none of these are set, the box is empty and absolute movement should be
 * relative to the extents of the layout.
 */
static void get_mapping(struct wlr_cursor *cur,
		struct wlr_input_device *dev, struct wlr_box *box) {
	assert(cur->state->layout);

	*box = (struct wlr_box){0};

	struct wlr_cursor_device *c_device = get_cursor_device(cur, dev);
	if (c_device) {
		if (!wlr_box_empty(&c_device->mapped_box)) {
			*box = c_device->mapped_box;
			return;
		}
		if (c_device->mapped_output) {
			wlr_output_layout_get_box(cur->state->layout,
				c_device->mapped_output, box);
			return;
		}
	}

	if (!wlr_box_empty(&cur->state->mapped_box)) {
		*box = cur->state->mapped_box;
		return;
	}
	if (cur->state->mapped_output) {
		wlr_output_layout_get_box(cur->state->layout,
			cur->state->mapped_output, box);
		return;
	}
}

bool wlr_cursor_warp(struct wlr_cursor *cur, struct wlr_input_device *dev,
		double lx, double ly) {
	assert(cur->state->layout);

	bool result = false;
	struct wlr_box mapping;
	get_mapping(cur, dev, &mapping);
	if (!wlr_box_empty(&mapping)) {
		result = wlr_box_contains_point(&mapping, lx, ly);
	} else {
		result = wlr_output_layout_contains_point(cur->state->layout, NULL,
			lx, ly);
	}

	if (result) {
		cursor_warp_unchecked(cur, lx, ly);
	}

	return result;
}

void wlr_cursor_warp_closest(struct wlr_cursor *cur,
		struct wlr_input_device *dev, double lx, double ly) {
	struct wlr_box mapping;
	get_mapping(cur, dev, &mapping);
	if (!wlr_box_empty(&mapping)) {
		wlr_box_closest_point(&mapping, lx, ly, &lx, &ly);
	} else if (!wl_list_empty(&cur->state->layout->outputs)) {
		wlr_output_layout_closest_point(cur->state->layout, NULL, lx, ly,
			&lx, &ly);
	} else {
		/*
		 * There is no mapping box for the input device and the
		 * output layout is empty.  This can happen for example
		 * when external monitors are turned off/disconnected.
		 * In this case, all (x,y) points are equally invalid,
		 * so leave the cursor in its current location (better
		 * from a user standpoint than warping it to (0,0)).
		 */
		return;
	}

	cursor_warp_unchecked(cur, lx, ly);
}

void wlr_cursor_absolute_to_layout_coords(struct wlr_cursor *cur,
		struct wlr_input_device *dev, double x, double y,
		double *lx, double *ly) {
	assert(cur->state->layout);

	struct wlr_box mapping;
	get_mapping(cur, dev, &mapping);
	if (wlr_box_empty(&mapping)) {
		wlr_output_layout_get_box(cur->state->layout, NULL, &mapping);
	}

	*lx = !isnan(x) ? mapping.width * x + mapping.x : cur->x;
	*ly = !isnan(y) ? mapping.height * y + mapping.y : cur->y;
}

void wlr_cursor_warp_absolute(struct wlr_cursor *cur,
		struct wlr_input_device *dev, double x, double y) {
	assert(cur->state->layout);

	double lx, ly;
	wlr_cursor_absolute_to_layout_coords(cur, dev, x, y, &lx, &ly);

	wlr_cursor_warp_closest(cur, dev, lx, ly);
}

void wlr_cursor_move(struct wlr_cursor *cur, struct wlr_input_device *dev,
		double delta_x, double delta_y) {
	assert(cur->state->layout);

	double lx = !isnan(delta_x) ? cur->x + delta_x : cur->x;
	double ly = !isnan(delta_y) ? cur->y + delta_y : cur->y;

	wlr_cursor_warp_closest(cur, dev, lx, ly);
}

static void cursor_output_cursor_reset_image(struct wlr_cursor_output_cursor *output_cursor) {
	output_cursor->xcursor = NULL;
	output_cursor->xcursor_index = 0;
	if (output_cursor->xcursor_timer != NULL) {
		wl_event_source_remove(output_cursor->xcursor_timer);
	}
	output_cursor->xcursor_timer = NULL;
}

static void cursor_update_outputs(struct wlr_cursor *cur);

void wlr_cursor_set_buffer(struct wlr_cursor *cur, struct wlr_buffer *buffer,
		int32_t hotspot_x, int32_t hotspot_y, float scale) {
	if (buffer == cur->state->buffer &&
			hotspot_x == cur->state->buffer_hotspot.x &&
			hotspot_y == cur->state->buffer_hotspot.y &&
			scale == cur->state->buffer_scale) {
		return;
	}

	cursor_reset_image(cur);

	if (buffer != NULL) {
		cur->state->buffer = wlr_buffer_lock(buffer);
		cur->state->buffer_hotspot.x = hotspot_x;
		cur->state->buffer_hotspot.y = hotspot_y;
		cur->state->buffer_scale = scale;
	}

	cursor_update_outputs(cur);
}

void wlr_cursor_unset_image(struct wlr_cursor *cur) {
	cursor_reset_image(cur);
	cursor_update_outputs(cur);
}

static void output_cursor_set_xcursor_image(struct wlr_cursor_output_cursor *output_cursor, size_t i);

static int handle_xcursor_timer(void *data) {
	struct wlr_cursor_output_cursor *output_cursor = data;
	size_t i = (output_cursor->xcursor_index + 1) % output_cursor->xcursor->image_count;
	output_cursor_set_xcursor_image(output_cursor, i);
	return 0;
}

static void output_cursor_set_xcursor_image(struct wlr_cursor_output_cursor *output_cursor, size_t i) {
	struct wlr_xcursor_image *image = output_cursor->xcursor->images[i];

	struct wlr_readonly_data_buffer *ro_buffer = readonly_data_buffer_create(
		DRM_FORMAT_ARGB8888, 4 * image->width, image->width, image->height, image->buffer);
	if (ro_buffer == NULL) {
		return;
	}
	wlr_output_cursor_set_buffer(output_cursor->output_cursor, &ro_buffer->base, image->hotspot_x, image->hotspot_y);
	wlr_buffer_drop(&ro_buffer->base);

	output_cursor->xcursor_index = i;

	if (output_cursor->xcursor->image_count == 1 || image->delay == 0) {
		return;
	}

	if (output_cursor->xcursor_timer == NULL) {
		struct wl_event_loop *event_loop = output_cursor->output_cursor->output->event_loop;
		output_cursor->xcursor_timer =
			wl_event_loop_add_timer(event_loop, handle_xcursor_timer, output_cursor);
		if (output_cursor->xcursor_timer == NULL) {
			wlr_log(WLR_ERROR, "wl_event_loop_add_timer failed");
			return;
		}
	}

	wl_event_source_timer_update(output_cursor->xcursor_timer, image->delay);
}

static void cursor_output_cursor_update(struct wlr_cursor_output_cursor *output_cursor) {
	struct wlr_cursor *cur = output_cursor->cursor;
	struct wlr_output *output = output_cursor->output_cursor->output;

	cursor_output_cursor_reset_image(output_cursor);

	if (cur->state->buffer != NULL) {
		struct wlr_renderer *renderer = output->renderer;
		assert(renderer != NULL);

		struct wlr_buffer *buffer = cur->state->buffer;
		int32_t hotspot_x = cur->state->buffer_hotspot.x;
		int32_t hotspot_y = cur->state->buffer_hotspot.y;
		float scale = cur->state->buffer_scale;

		struct wlr_texture *texture = NULL;
		struct wlr_fbox src_box = {0};
		int dst_width = 0, dst_height = 0;
		if (buffer != NULL) {
			texture = wlr_texture_from_buffer(renderer, buffer);
			if (texture) {
				src_box = (struct wlr_fbox){
					.width = texture->width,
					.height = texture->height,
				};

				dst_width = texture->width / scale;
				dst_height = texture->height / scale;
			}
		}

		output_cursor_set_texture(output_cursor->output_cursor, texture, true,
			&src_box, dst_width, dst_height, WL_OUTPUT_TRANSFORM_NORMAL,
			hotspot_x, hotspot_y, NULL, 0);
	} else if (cur->state->surface != NULL) {
		struct wlr_surface *surface = cur->state->surface;

		struct wlr_texture *texture = wlr_surface_get_texture(surface);
		int32_t hotspot_x = cur->state->surface_hotspot.x;
		int32_t hotspot_y = cur->state->surface_hotspot.y;

		struct wlr_fbox src_box;
		wlr_surface_get_buffer_source_box(surface, &src_box);
		int dst_width = surface->current.width;
		int dst_height = surface->current.height;

		struct wlr_linux_drm_syncobj_surface_v1_state *syncobj_surface_state =
			wlr_linux_drm_syncobj_v1_get_surface_state(surface);
		struct wlr_drm_syncobj_timeline *wait_timeline = NULL;
		uint64_t wait_point = 0;
		if (syncobj_surface_state != NULL) {
			wait_timeline = syncobj_surface_state->acquire_timeline;
			wait_point = syncobj_surface_state->acquire_point;
		}

		output_cursor_set_texture(output_cursor->output_cursor, texture, false,
			&src_box, dst_width, dst_height, surface->current.transform,
			hotspot_x, hotspot_y, wait_timeline, wait_point);

		if (syncobj_surface_state != NULL &&
				surface->buffer != NULL && surface->buffer->source != NULL &&
				(surface->current.committed & WLR_SURFACE_STATE_BUFFER)) {
			wlr_linux_drm_syncobj_v1_state_signal_release_with_buffer(syncobj_surface_state,
				surface->buffer->source);
		}

		if (output_cursor->output_cursor->visible) {
			wlr_surface_send_enter(surface, output);
		} else {
			wlr_surface_send_leave(surface, output);
		}

		float scale = 1;
		struct wlr_surface_output *surface_output;
		wl_list_for_each(surface_output, &surface->current_outputs, link) {
			if (surface_output->output->scale > scale) {
				scale = surface_output->output->scale;
			}
		}
		wlr_fractional_scale_v1_notify_scale(surface, scale);
		wlr_surface_set_preferred_buffer_scale(surface, ceil(scale));
	} else if (cur->state->xcursor_name != NULL) {
		struct wlr_xcursor_manager *manager = cur->state->xcursor_manager;
		const char *name = cur->state->xcursor_name;

		float scale = output->scale;
		wlr_xcursor_manager_load(manager, scale);
		struct wlr_xcursor *xcursor = wlr_xcursor_manager_get_xcursor(manager, name, scale);
		if (xcursor == NULL) {
			/* Try the default cursor: better the wrong image than an invisible
			 * (and therefore practically unusable) cursor */
			wlr_log(WLR_DEBUG, "XCursor theme is missing '%s' cursor, falling back to 'default'", name);
			xcursor = wlr_xcursor_manager_get_xcursor(manager, "default", scale);
			if (xcursor == NULL) {
				wlr_log(WLR_DEBUG, "XCursor theme is missing a 'default' cursor");
				wlr_output_cursor_set_buffer(output_cursor->output_cursor, NULL, 0, 0);
				return;
			}
		}

		output_cursor->xcursor = xcursor;
		output_cursor_set_xcursor_image(output_cursor, 0);
	} else {
		wlr_output_cursor_set_buffer(output_cursor->output_cursor, NULL, 0, 0);
	}
}

static void output_cursor_output_handle_output_commit(
		struct wl_listener *listener, void *data) {
	struct wlr_cursor_output_cursor *output_cursor =
		wl_container_of(listener, output_cursor, output_commit);
	const struct wlr_output_event_commit *event = data;

	if (event->state->committed & (WLR_OUTPUT_STATE_SCALE | WLR_OUTPUT_STATE_TRANSFORM
			| WLR_OUTPUT_STATE_ENABLED | WLR_OUTPUT_STATE_IMAGE_DESCRIPTION)) {
		cursor_output_cursor_update(output_cursor);
	}

	struct wlr_surface *surface = output_cursor->cursor->state->surface;
	if (surface && output_cursor->output_cursor->visible &&
			(event->state->committed & WLR_OUTPUT_STATE_BUFFER)) {
		wlr_surface_send_frame_done(surface, &event->when);
	}
}

static void cursor_update_outputs(struct wlr_cursor *cur) {
	struct wlr_cursor_output_cursor *output_cursor;
	wl_list_for_each(output_cursor, &cur->state->output_cursors, link) {
		cursor_output_cursor_update(output_cursor);
	}
}

void wlr_cursor_set_xcursor(struct wlr_cursor *cur,
		struct wlr_xcursor_manager *manager, const char *name) {
	if (manager == cur->state->xcursor_manager &&
			cur->state->xcursor_name != NULL &&
			strcmp(name, cur->state->xcursor_name) == 0) {
		return;
	}

	cursor_reset_image(cur);

	cur->state->xcursor_manager = manager;
	cur->state->xcursor_name = strdup(name);

	cursor_update_outputs(cur);
}

static void cursor_handle_surface_destroy(struct wl_listener *listener, void *data) {
	struct wlr_cursor_state *state = wl_container_of(listener, state, surface_destroy);
	assert(state->surface != NULL);
	wlr_cursor_unset_image(&state->cursor);
}

static void cursor_handle_surface_commit(struct wl_listener *listener, void *data) {
	struct wlr_cursor_state *state = wl_container_of(listener, state, surface_commit);
	struct wlr_surface *surface = state->surface;

	state->surface_hotspot.x -= surface->current.dx;
	state->surface_hotspot.y -= surface->current.dy;

	cursor_update_outputs(&state->cursor);
}

void wlr_cursor_set_surface(struct wlr_cursor *cur, struct wlr_surface *surface,
		int32_t hotspot_x, int32_t hotspot_y) {
	if (surface == NULL) {
		wlr_cursor_unset_image(cur);
		return;
	}

	if (surface == cur->state->surface &&
			hotspot_x == cur->state->surface_hotspot.x &&
			hotspot_y == cur->state->surface_hotspot.y) {
		return;
	}

	if (surface != cur->state->surface) {
		// Only send wl_surface.leave if the surface changes
		cursor_reset_image(cur);

		cur->state->surface = surface;

		wl_signal_add(&surface->events.destroy, &cur->state->surface_destroy);
		cur->state->surface_destroy.notify = cursor_handle_surface_destroy;
		wl_signal_add(&surface->events.commit, &cur->state->surface_commit);
		cur->state->surface_commit.notify = cursor_handle_surface_commit;
	}

	cur->state->surface_hotspot.x = hotspot_x;
	cur->state->surface_hotspot.y = hotspot_y;

	cursor_update_outputs(cur);
}

static void handle_pointer_motion(struct wl_listener *listener, void *data) {
	struct wlr_pointer_motion_event *event = data;
	struct wlr_cursor_device *device =
		wl_container_of(listener, device, motion);
	wl_signal_emit_mutable(&device->cursor->events.motion, event);
}

static void apply_output_transform(double *x, double *y,
		enum wl_output_transform transform) {
	double dx = 0.0, dy = 0.0;
	double width = 1.0, height = 1.0;

	switch (transform) {
	case WL_OUTPUT_TRANSFORM_NORMAL:
		dx = *x;
		dy = *y;
		break;
	case WL_OUTPUT_TRANSFORM_90:
		dx = height - *y;
		dy = *x;
		break;
	case WL_OUTPUT_TRANSFORM_180:
		dx = width - *x;
		dy = height - *y;
		break;
	case WL_OUTPUT_TRANSFORM_270:
		dx = *y;
		dy = width - *x;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED:
		dx = width - *x;
		dy = *y;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_90:
		dx = *y;
		dy = *x;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_180:
		dx = *x;
		dy = height - *y;
		break;
	case WL_OUTPUT_TRANSFORM_FLIPPED_270:
		dx = height - *y;
		dy = width - *x;
		break;
	}
	*x = dx;
	*y = dy;
}


static struct wlr_output *get_mapped_output(struct wlr_cursor_device *cursor_device) {
	if (cursor_device->mapped_output) {
		return cursor_device->mapped_output;
	}

	struct wlr_cursor *cursor = cursor_device->cursor;
	assert(cursor);
	if (cursor->state->mapped_output) {
		return cursor->state->mapped_output;
	}
	return NULL;
}


static void handle_pointer_motion_absolute(struct wl_listener *listener,
		void *data) {
	struct wlr_pointer_motion_absolute_event *event = data;
	struct wlr_cursor_device *device =
		wl_container_of(listener, device, motion_absolute);

	struct wlr_output *output =
		get_mapped_output(device);
	if (output) {
		apply_output_transform(&event->x, &event->y, output->transform);
	}
	wl_signal_emit_mutable(&device->cursor->events.motion_absolute, event);
}

static void handle_pointer_button(struct wl_listener *listener, void *data) {
	struct wlr_pointer_button_event *event = data;
	struct wlr_cursor_device *device =
		wl_container_of(listener, device, button);
	wl_signal_emit_mutable(&device->cursor->events.button, event);
}

static void handle_pointer_axis(struct wl_listener *listener, void *data) {
	struct wlr_pointer_axis_event *event = data;
	struct wlr_cursor_device *device = wl_container_of(listener, device, axis);
	wl_signal_emit_mutable(&device->cursor->events.axis, event);
}

static void handle_pointer_frame(struct wl_listener *listener, void *data) {
	struct wlr_cursor_device *device = wl_container_of(listener, device, frame);
	wl_signal_emit_mutable(&device->cursor->events.frame, device->cursor);
}

static void handle_pointer_swipe_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_begin_event *event = data;
	struct wlr_cursor_device *device = wl_container_of(listener, device, swipe_begin);
	wl_signal_emit_mutable(&device->cursor->events.swipe_begin, event);
}

static void handle_pointer_swipe_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_update_event *event = data;
	struct wlr_cursor_device *device = wl_container_of(listener, device, swipe_update);
	wl_signal_emit_mutable(&device->cursor->events.swipe_update, event);
}

static void handle_pointer_swipe_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_swipe_end_event *event = data;
	struct wlr_cursor_device *device = wl_container_of(listener, device, swipe_end);
	wl_signal_emit_mutable(&device->cursor->events.swipe_end, event);
}

static void handle_pointer_pinch_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_begin_event *event = data;
	struct wlr_cursor_device *device = wl_container_of(listener, device, pinch_begin);
	wl_signal_emit_mutable(&device->cursor->events.pinch_begin, event);
}

static void handle_pointer_pinch_update(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_update_event *event = data;
	struct wlr_cursor_device *device = wl_container_of(listener, device, pinch_update);
	wl_signal_emit_mutable(&device->cursor->events.pinch_update, event);
}

static void handle_pointer_pinch_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_pinch_end_event *event = data;
	struct wlr_cursor_device *device = wl_container_of(listener, device, pinch_end);
	wl_signal_emit_mutable(&device->cursor->events.pinch_end, event);
}

static void handle_pointer_hold_begin(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_begin_event *event = data;
	struct wlr_cursor_device *device = wl_container_of(listener, device, hold_begin);
	wl_signal_emit_mutable(&device->cursor->events.hold_begin, event);
}

static void handle_pointer_hold_end(struct wl_listener *listener, void *data) {
	struct wlr_pointer_hold_end_event *event = data;
	struct wlr_cursor_device *device = wl_container_of(listener, device, hold_end);
	wl_signal_emit_mutable(&device->cursor->events.hold_end, event);
}

static void handle_touch_up(struct wl_listener *listener, void *data) {
	struct wlr_touch_up_event *event = data;
	struct wlr_cursor_device *device;
	device = wl_container_of(listener, device, touch_up);
	wl_signal_emit_mutable(&device->cursor->events.touch_up, event);
}

static void handle_touch_down(struct wl_listener *listener, void *data) {
	struct wlr_touch_down_event *event = data;
	struct wlr_cursor_device *device;
	device = wl_container_of(listener, device, touch_down);

	struct wlr_output *output =
		get_mapped_output(device);
	if (output) {
		apply_output_transform(&event->x, &event->y, output->transform);
	}
	wl_signal_emit_mutable(&device->cursor->events.touch_down, event);
}

static void handle_touch_motion(struct wl_listener *listener, void *data) {
	struct wlr_touch_motion_event *event = data;
	struct wlr_cursor_device *device;
	device = wl_container_of(listener, device, touch_motion);

	struct wlr_output *output =
		get_mapped_output(device);
	if (output) {
		apply_output_transform(&event->x, &event->y, output->transform);
	}
	wl_signal_emit_mutable(&device->cursor->events.touch_motion, event);
}

static void handle_touch_cancel(struct wl_listener *listener, void *data) {
	struct wlr_touch_cancel_event *event = data;
	struct wlr_cursor_device *device;
	device = wl_container_of(listener, device, touch_cancel);
	wl_signal_emit_mutable(&device->cursor->events.touch_cancel, event);
}

static void handle_touch_frame(struct wl_listener *listener, void *data) {
	struct wlr_cursor_device *device =
		wl_container_of(listener, device, touch_frame);
	wl_signal_emit_mutable(&device->cursor->events.touch_frame, NULL);
}

static void handle_tablet_tool_tip(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_tip_event *event = data;
	struct wlr_cursor_device *device;
	device = wl_container_of(listener, device, tablet_tool_tip);

	struct wlr_output *output =
		get_mapped_output(device);
	if (output) {
		apply_output_transform(&event->x, &event->y, output->transform);
	}
	wl_signal_emit_mutable(&device->cursor->events.tablet_tool_tip, event);
}

static void handle_tablet_tool_axis(struct wl_listener *listener, void *data) {
	struct wlr_tablet_tool_axis_event *event = data;
	struct wlr_cursor_device *device;
	device = wl_container_of(listener, device, tablet_tool_axis);

	struct wlr_output *output = get_mapped_output(device);
	if (output) {
		// In the case that only one axis received an event, rotating the input can
		// cause the change to actually happen on the other axis, as far as clients
		// are concerned.
		//
		// Here, we feed apply_output_transform NAN on the axis that didn't change,
		// and remap the axes flags based on whether it returns NAN itself.
		double x = event->updated_axes & WLR_TABLET_TOOL_AXIS_X ? event->x : NAN;
		double y = event->updated_axes & WLR_TABLET_TOOL_AXIS_Y ? event->y : NAN;

		apply_output_transform(&x, &y, output->transform);

		event->updated_axes &= ~(WLR_TABLET_TOOL_AXIS_X | WLR_TABLET_TOOL_AXIS_Y);
		event->x = event->y = 0;

		if (!isnan(x)) {
			event->updated_axes |= WLR_TABLET_TOOL_AXIS_X;
			event->x = x;
		}

		if (!isnan(y)) {
			event->updated_axes |= WLR_TABLET_TOOL_AXIS_Y;
			event->y = y;
		}
	}

	wl_signal_emit_mutable(&device->cursor->events.tablet_tool_axis, event);
}

static void handle_tablet_tool_button(struct wl_listener *listener,
		void *data) {
	struct wlr_tablet_tool_button *event = data;
	struct wlr_cursor_device *device;
	device = wl_container_of(listener, device, tablet_tool_button);
	wl_signal_emit_mutable(&device->cursor->events.tablet_tool_button, event);
}

static void handle_tablet_tool_proximity(struct wl_listener *listener,
		void *data) {
	struct wlr_tablet_tool_proximity_event *event = data;
	struct wlr_cursor_device *device;
	device = wl_container_of(listener, device, tablet_tool_proximity);

	struct wlr_output *output =
		get_mapped_output(device);
	if (output) {
		apply_output_transform(&event->x, &event->y, output->transform);
	}
	wl_signal_emit_mutable(&device->cursor->events.tablet_tool_proximity, event);
}

static void handle_device_destroy(struct wl_listener *listener, void *data) {
	struct wlr_cursor_device *c_device;
	c_device = wl_container_of(listener, c_device, destroy);
	wlr_cursor_detach_input_device(c_device->cursor, c_device->device);
}

static struct wlr_cursor_device *cursor_device_create(
		struct wlr_cursor *cursor, struct wlr_input_device *device) {
	struct wlr_cursor_device *c_device = calloc(1, sizeof(*c_device));
	if (!c_device) {
		wlr_log(WLR_ERROR, "Failed to allocate wlr_cursor_device");
		return NULL;
	}

	c_device->cursor = cursor;
	c_device->device = device;

	// listen to events
	wl_signal_add(&device->events.destroy, &c_device->destroy);
	c_device->destroy.notify = handle_device_destroy;

	switch (device->type) {
	case WLR_INPUT_DEVICE_POINTER:;
		struct wlr_pointer *pointer = wlr_pointer_from_input_device(device);

		wl_signal_add(&pointer->events.motion, &c_device->motion);
		c_device->motion.notify = handle_pointer_motion;

		wl_signal_add(&pointer->events.motion_absolute,
			&c_device->motion_absolute);
		c_device->motion_absolute.notify = handle_pointer_motion_absolute;

		wl_signal_add(&pointer->events.button, &c_device->button);
		c_device->button.notify = handle_pointer_button;

		wl_signal_add(&pointer->events.axis, &c_device->axis);
		c_device->axis.notify = handle_pointer_axis;

		wl_signal_add(&pointer->events.frame, &c_device->frame);
		c_device->frame.notify = handle_pointer_frame;

		wl_signal_add(&pointer->events.swipe_begin, &c_device->swipe_begin);
		c_device->swipe_begin.notify = handle_pointer_swipe_begin;

		wl_signal_add(&pointer->events.swipe_update, &c_device->swipe_update);
		c_device->swipe_update.notify = handle_pointer_swipe_update;

		wl_signal_add(&pointer->events.swipe_end, &c_device->swipe_end);
		c_device->swipe_end.notify = handle_pointer_swipe_end;

		wl_signal_add(&pointer->events.pinch_begin, &c_device->pinch_begin);
		c_device->pinch_begin.notify = handle_pointer_pinch_begin;

		wl_signal_add(&pointer->events.pinch_update, &c_device->pinch_update);
		c_device->pinch_update.notify = handle_pointer_pinch_update;

		wl_signal_add(&pointer->events.pinch_end, &c_device->pinch_end);
		c_device->pinch_end.notify = handle_pointer_pinch_end;

		wl_signal_add(&pointer->events.hold_begin, &c_device->hold_begin);
		c_device->hold_begin.notify = handle_pointer_hold_begin;

		wl_signal_add(&pointer->events.hold_end, &c_device->hold_end);
		c_device->hold_end.notify = handle_pointer_hold_end;

		break;
	case WLR_INPUT_DEVICE_TOUCH:;
		struct wlr_touch *touch = wlr_touch_from_input_device(device);

		wl_signal_add(&touch->events.motion, &c_device->touch_motion);
		c_device->touch_motion.notify = handle_touch_motion;

		wl_signal_add(&touch->events.down, &c_device->touch_down);
		c_device->touch_down.notify = handle_touch_down;

		wl_signal_add(&touch->events.up, &c_device->touch_up);
		c_device->touch_up.notify = handle_touch_up;

		wl_signal_add(&touch->events.cancel, &c_device->touch_cancel);
		c_device->touch_cancel.notify = handle_touch_cancel;

		wl_signal_add(&touch->events.frame, &c_device->touch_frame);
		c_device->touch_frame.notify = handle_touch_frame;

		break;
	case WLR_INPUT_DEVICE_TABLET:;
		struct wlr_tablet *tablet = wlr_tablet_from_input_device(device);

		wl_signal_add(&tablet->events.tip, &c_device->tablet_tool_tip);
		c_device->tablet_tool_tip.notify = handle_tablet_tool_tip;

		wl_signal_add(&tablet->events.proximity,
			&c_device->tablet_tool_proximity);
		c_device->tablet_tool_proximity.notify = handle_tablet_tool_proximity;

		wl_signal_add(&tablet->events.axis, &c_device->tablet_tool_axis);
		c_device->tablet_tool_axis.notify = handle_tablet_tool_axis;

		wl_signal_add(&tablet->events.button, &c_device->tablet_tool_button);
		c_device->tablet_tool_button.notify = handle_tablet_tool_button;

		break;

	default:
		abort(); // unreachable
	}

	wl_list_insert(&cursor->state->devices, &c_device->link);

	return c_device;
}

void wlr_cursor_attach_input_device(struct wlr_cursor *cur,
		struct wlr_input_device *dev) {
	switch (dev->type) {
	case WLR_INPUT_DEVICE_POINTER:
	case WLR_INPUT_DEVICE_TOUCH:
	case WLR_INPUT_DEVICE_TABLET:
		break;
	default:
		wlr_log(WLR_ERROR, "only device types of pointer, touch or tablet tool"
				"are supported");
		return;
	}

	// make sure it is not already attached
	struct wlr_cursor_device *_dev;
	wl_list_for_each(_dev, &cur->state->devices, link) {
		if (_dev->device == dev) {
			return;
		}
	}

	cursor_device_create(cur, dev);
}

void wlr_cursor_detach_input_device(struct wlr_cursor *cur,
		struct wlr_input_device *dev) {
	struct wlr_cursor_device *c_device, *tmp = NULL;
	wl_list_for_each_safe(c_device, tmp, &cur->state->devices, link) {
		if (c_device->device == dev) {
			cursor_device_destroy(c_device);
		}
	}
}

static void handle_layout_destroy(struct wl_listener *listener, void *data) {
	struct wlr_cursor_state *state =
		wl_container_of(listener, state, layout_destroy);
	cursor_detach_output_layout(&state->cursor);
}

static void handle_layout_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_cursor_output_cursor *output_cursor =
		wl_container_of(listener, output_cursor, layout_output_destroy);
	output_cursor_destroy(output_cursor);
}

static void layout_add(struct wlr_cursor_state *state,
		struct wlr_output_layout_output *l_output) {
	struct wlr_cursor_output_cursor *output_cursor;
	wl_list_for_each(output_cursor, &state->output_cursors, link) {
		if (output_cursor->output_cursor->output == l_output->output) {
			return; // already added
		}
	}

	output_cursor = calloc(1, sizeof(*output_cursor));
	if (output_cursor == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate wlr_cursor_output_cursor");
		return;
	}
	output_cursor->cursor = &state->cursor;

	output_cursor->output_cursor = wlr_output_cursor_create(l_output->output);
	if (output_cursor->output_cursor == NULL) {
		wlr_log(WLR_ERROR, "Failed to create wlr_output_cursor");
		free(output_cursor);
		return;
	}

	output_cursor->layout_output_destroy.notify = handle_layout_output_destroy;
	wl_signal_add(&l_output->events.destroy,
		&output_cursor->layout_output_destroy);

	wl_list_insert(&state->output_cursors, &output_cursor->link);

	wl_signal_add(&output_cursor->output_cursor->output->events.commit,
		&output_cursor->output_commit);
	output_cursor->output_commit.notify = output_cursor_output_handle_output_commit;

	output_cursor_move(output_cursor);
	cursor_output_cursor_update(output_cursor);
}

static void handle_layout_add(struct wl_listener *listener, void *data) {
	struct wlr_cursor_state *state =
		wl_container_of(listener, state, layout_add);
	struct wlr_output_layout_output *l_output = data;
	layout_add(state, l_output);
}

static void handle_layout_change(struct wl_listener *listener, void *data) {
	struct wlr_cursor_state *state =
		wl_container_of(listener, state, layout_change);
	struct wlr_output_layout *layout = data;

	if (!wlr_output_layout_contains_point(layout, NULL, state->cursor.x,
			state->cursor.y) && !wl_list_empty(&layout->outputs)) {
		// the output we were on has gone away so go to the closest boundary
		// point (unless the layout is empty; compare warp_closest())
		double x, y;
		wlr_output_layout_closest_point(layout, NULL, state->cursor.x,
			state->cursor.y, &x, &y);

		cursor_warp_unchecked(&state->cursor, x, y);
	}
}

void wlr_cursor_attach_output_layout(struct wlr_cursor *cur,
		struct wlr_output_layout *l) {
	cursor_detach_output_layout(cur);

	if (l == NULL) {
		return;
	}

	wl_signal_add(&l->events.add, &cur->state->layout_add);
	cur->state->layout_add.notify = handle_layout_add;
	wl_signal_add(&l->events.change, &cur->state->layout_change);
	cur->state->layout_change.notify = handle_layout_change;
	wl_signal_add(&l->events.destroy, &cur->state->layout_destroy);
	cur->state->layout_destroy.notify = handle_layout_destroy;

	cur->state->layout = l;

	struct wlr_output_layout_output *l_output;
	wl_list_for_each(l_output, &l->outputs, link) {
		layout_add(cur->state, l_output);
	}
}

void wlr_cursor_map_to_output(struct wlr_cursor *cur,
		struct wlr_output *output) {
	cur->state->mapped_output = output;
}

void wlr_cursor_map_input_to_output(struct wlr_cursor *cur,
		struct wlr_input_device *dev, struct wlr_output *output) {
	struct wlr_cursor_device *c_device = get_cursor_device(cur, dev);
	if (!c_device) {
		wlr_log(WLR_ERROR, "Cannot map device \"%s\" to output"
			" (not found in this cursor)", dev->name);
		return;
	}

	c_device->mapped_output = output;
}

void wlr_cursor_map_to_region(struct wlr_cursor *cur,
		const struct wlr_box *box) {
	cur->state->mapped_box = wlr_box_empty(box) ? (struct wlr_box){0} : *box;
}

void wlr_cursor_map_input_to_region(struct wlr_cursor *cur,
		struct wlr_input_device *dev, const struct wlr_box *box) {
	struct wlr_cursor_device *c_device = get_cursor_device(cur, dev);
	if (!c_device) {
		wlr_log(WLR_ERROR, "Cannot map device \"%s\" to geometry (not found in"
			"this cursor)", dev->name);
		return;
	}

	c_device->mapped_box = wlr_box_empty(box) ? (struct wlr_box){0} : *box;
}
