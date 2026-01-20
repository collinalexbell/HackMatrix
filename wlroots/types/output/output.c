#include <assert.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <wlr/backend.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output_layer.h>
#include <wlr/util/log.h>
#include "types/wlr_output.h"
#include "util/env.h"
#include "util/global.h"

#define OUTPUT_VERSION 4

static void send_geometry(struct wl_resource *resource) {
	struct wlr_output *output = wlr_output_from_resource(resource);
	wl_output_send_geometry(resource, 0, 0,
		output->phys_width, output->phys_height, output->subpixel,
		"Unknown", "Unknown", output->transform);
}

static void send_current_mode(struct wl_resource *resource) {
	struct wlr_output *output = wlr_output_from_resource(resource);
	if (output->current_mode != NULL) {
		struct wlr_output_mode *mode = output->current_mode;
		wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT,
			mode->width, mode->height, mode->refresh);
	} else {
		// Output has no mode
		wl_output_send_mode(resource, WL_OUTPUT_MODE_CURRENT, output->width,
			output->height, output->refresh);
	}
}

static void send_scale(struct wl_resource *resource) {
	struct wlr_output *output = wlr_output_from_resource(resource);
	uint32_t version = wl_resource_get_version(resource);
	if (version >= WL_OUTPUT_SCALE_SINCE_VERSION) {
		wl_output_send_scale(resource, (uint32_t)ceil(output->scale));
	}
}

static void send_name(struct wl_resource *resource) {
	struct wlr_output *output = wlr_output_from_resource(resource);
	uint32_t version = wl_resource_get_version(resource);
	if (version >= WL_OUTPUT_NAME_SINCE_VERSION) {
		wl_output_send_name(resource, output->name);
	}
}

static void send_description(struct wl_resource *resource) {
	struct wlr_output *output = wlr_output_from_resource(resource);
	uint32_t version = wl_resource_get_version(resource);
	if (output->description != NULL &&
			version >= WL_OUTPUT_DESCRIPTION_SINCE_VERSION) {
		wl_output_send_description(resource, output->description);
	}
}

static void send_done(struct wl_resource *resource) {
	uint32_t version = wl_resource_get_version(resource);
	if (version >= WL_OUTPUT_DONE_SINCE_VERSION) {
		wl_output_send_done(resource);
	}
}

static void output_handle_resource_destroy(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static void output_handle_release(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wl_output_interface output_impl = {
	.release = output_handle_release,
};

static void output_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id) {
	// `output` can be NULL if the output global is being destroyed
	struct wlr_output *output = data;

	struct wl_resource *resource = wl_resource_create(wl_client,
		&wl_output_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	wl_resource_set_implementation(resource, &output_impl, output,
		output_handle_resource_destroy);

	if (output == NULL) {
		wl_list_init(wl_resource_get_link(resource));
		return;
	}

	wl_list_insert(&output->resources, wl_resource_get_link(resource));

	send_geometry(resource);
	send_current_mode(resource);
	send_scale(resource);
	send_name(resource);
	send_description(resource);
	send_done(resource);

	struct wlr_output_event_bind evt = {
		.output = output,
		.resource = resource,
	};

	wl_signal_emit_mutable(&output->events.bind, &evt);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_output *output = wl_container_of(listener, output, display_destroy);
	wlr_output_destroy_global(output);
}

void wlr_output_create_global(struct wlr_output *output, struct wl_display *display) {
	if (output->global != NULL) {
		return;
	}

	output->global = wl_global_create(display,
		&wl_output_interface, OUTPUT_VERSION, output, output_bind);
	if (output->global == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate wl_output global");
		return;
	}

	wl_list_remove(&output->display_destroy.link);
	wl_display_add_destroy_listener(display, &output->display_destroy);
}

void wlr_output_destroy_global(struct wlr_output *output) {
	if (output->global == NULL) {
		return;
	}

	// Make all output resources inert
	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &output->resources) {
		wl_resource_set_user_data(resource, NULL);
		wl_list_remove(wl_resource_get_link(resource));
		wl_list_init(wl_resource_get_link(resource));
	}

	wl_list_remove(&output->display_destroy.link);
	wl_list_init(&output->display_destroy.link);

	wlr_global_destroy_safe(output->global);
	output->global = NULL;
}

static void schedule_done_handle_idle_timer(void *data) {
	struct wlr_output *output = data;
	output->idle_done = NULL;

	struct wl_resource *resource;
	wl_resource_for_each(resource, &output->resources) {
		send_done(resource);
	}
}

void wlr_output_schedule_done(struct wlr_output *output) {
	if (output->idle_done != NULL) {
		return; // Already scheduled
	}

	output->idle_done = wl_event_loop_add_idle(output->event_loop,
		schedule_done_handle_idle_timer, output);
}

struct wlr_output *wlr_output_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wl_output_interface,
		&output_impl));
	return wl_resource_get_user_data(resource);
}

void wlr_output_set_name(struct wlr_output *output, const char *name) {
	assert(output->global == NULL);

	free(output->name);
	output->name = strdup(name);
}

void wlr_output_set_description(struct wlr_output *output, const char *desc) {
	if (output->description != NULL && desc != NULL &&
			strcmp(output->description, desc) == 0) {
		return;
	}

	free(output->description);
	if (desc != NULL) {
		output->description = strdup(desc);
	} else {
		output->description = NULL;
	}

	struct wl_resource *resource;
	wl_resource_for_each(resource, &output->resources) {
		send_description(resource);
	}
	wlr_output_schedule_done(output);

	wl_signal_emit_mutable(&output->events.description, output);
}

static void output_apply_state(struct wlr_output *output,
		const struct wlr_output_state *state) {
	if (state->committed & WLR_OUTPUT_STATE_RENDER_FORMAT) {
		output->render_format = state->render_format;
	}

	if (state->committed & WLR_OUTPUT_STATE_SUBPIXEL) {
		output->subpixel = state->subpixel;
	}

	if (state->committed & WLR_OUTPUT_STATE_ENABLED) {
		output->enabled = state->enabled;
	}

	bool scale_updated = state->committed & WLR_OUTPUT_STATE_SCALE;
	if (scale_updated) {
		output->scale = state->scale;
	}

	if (state->committed & WLR_OUTPUT_STATE_TRANSFORM) {
		output->transform = state->transform;
	}

	if (state->committed & WLR_OUTPUT_STATE_IMAGE_DESCRIPTION) {
		if (state->image_description != NULL) {
			output->image_description_value = *state->image_description;
			output->image_description = &output->image_description_value;
		} else {
			output->image_description = NULL;
		}

		struct wlr_output_cursor *output_cursor;
		wl_list_for_each(output_cursor, &output->cursors, link) {
			output_cursor_refresh_color_transform(output_cursor, output->image_description);
		}
	}

	if (state->committed & WLR_OUTPUT_STATE_COLOR_TRANSFORM) {
		wlr_color_transform_unref(output->color_transform);
		if (state->color_transform != NULL) {
			output->color_transform = wlr_color_transform_ref(state->color_transform);
		} else {
			output->color_transform = NULL;
		}
	}

	bool geometry_updated = state->committed &
		(WLR_OUTPUT_STATE_MODE | WLR_OUTPUT_STATE_TRANSFORM |
		WLR_OUTPUT_STATE_SUBPIXEL);

	// Destroy the swapchains when an output is disabled
	if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && !state->enabled) {
		wlr_swapchain_destroy(output->swapchain);
		output->swapchain = NULL;
		wlr_swapchain_destroy(output->cursor_swapchain);
		output->cursor_swapchain = NULL;
	}

	if (state->committed & WLR_OUTPUT_STATE_LAYERS) {
		for (size_t i = 0; i < state->layers_len; i++) {
			struct wlr_output_layer_state *layer_state = &state->layers[i];
			struct wlr_output_layer *layer = layer_state->layer;

			// Commit layer ordering
			wl_list_remove(&layer->link);
			wl_list_insert(output->layers.prev, &layer->link);

			// Commit layer state
			layer->src_box = layer_state->src_box;
			layer->dst_box = layer_state->dst_box;
		}
	}

	bool mode_updated = false;
	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		int width = 0, height = 0, refresh = 0;
		switch (state->mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:;
			struct wlr_output_mode *mode = state->mode;
			output->current_mode = mode;
			if (mode != NULL) {
				width = mode->width;
				height = mode->height;
				refresh = mode->refresh;
			}
			break;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			output->current_mode = NULL;
			width = state->custom_mode.width;
			height = state->custom_mode.height;
			refresh = state->custom_mode.refresh;
			break;
		}

		if (output->width != width || output->height != height ||
				output->refresh != refresh) {
			output->width = width;
			output->height = height;

			output->refresh = refresh;

			if (output->swapchain != NULL &&
					(output->swapchain->width != output->width ||
					output->swapchain->height != output->height)) {
				wlr_swapchain_destroy(output->swapchain);
				output->swapchain = NULL;
			}

			mode_updated = true;
		}
	}

	if (geometry_updated || scale_updated || mode_updated) {
		struct wl_resource *resource;
		wl_resource_for_each(resource, &output->resources) {
			if (mode_updated) {
				send_current_mode(resource);
			}
			if (geometry_updated) {
				send_geometry(resource);
			}
			if (scale_updated) {
				send_scale(resource);
			}
		}
		wlr_output_schedule_done(output);
	}
}

void wlr_output_init(struct wlr_output *output, struct wlr_backend *backend,
		const struct wlr_output_impl *impl, struct wl_event_loop *event_loop,
		const struct wlr_output_state *state) {
	assert(impl->commit);
	if (impl->set_cursor || impl->move_cursor) {
		assert(impl->set_cursor && impl->move_cursor);
	}

	*output = (struct wlr_output){
		.backend = backend,
		.impl = impl,
		.event_loop = event_loop,
		.render_format = DRM_FORMAT_XRGB8888,
		.transform = WL_OUTPUT_TRANSFORM_NORMAL,
		.scale = 1,
		.commit_seq = 0,
	};

	wl_list_init(&output->modes);
	wl_list_init(&output->cursors);
	wl_list_init(&output->layers);
	wl_list_init(&output->resources);

	wl_signal_init(&output->events.frame);
	wl_signal_init(&output->events.damage);
	wl_signal_init(&output->events.needs_frame);
	wl_signal_init(&output->events.precommit);
	wl_signal_init(&output->events.commit);
	wl_signal_init(&output->events.present);
	wl_signal_init(&output->events.bind);
	wl_signal_init(&output->events.description);
	wl_signal_init(&output->events.request_state);
	wl_signal_init(&output->events.destroy);

	output->software_cursor_locks = env_parse_bool("WLR_NO_HARDWARE_CURSORS");
	if (output->software_cursor_locks) {
		wlr_log(WLR_DEBUG, "WLR_NO_HARDWARE_CURSORS set, forcing software cursors");
	}

	wlr_addon_set_init(&output->addons);

	wl_list_init(&output->display_destroy.link);
	output->display_destroy.notify = handle_display_destroy;

	if (state) {
		output_apply_state(output, state);
	}
}

void wlr_output_finish(struct wlr_output *output) {
	wl_signal_emit_mutable(&output->events.destroy, output);
	wlr_addon_set_finish(&output->addons);

	assert(wl_list_empty(&output->events.frame.listener_list));
	assert(wl_list_empty(&output->events.damage.listener_list));
	assert(wl_list_empty(&output->events.needs_frame.listener_list));
	assert(wl_list_empty(&output->events.precommit.listener_list));
	assert(wl_list_empty(&output->events.commit.listener_list));
	assert(wl_list_empty(&output->events.present.listener_list));
	assert(wl_list_empty(&output->events.bind.listener_list));
	assert(wl_list_empty(&output->events.description.listener_list));
	assert(wl_list_empty(&output->events.request_state.listener_list));
	assert(wl_list_empty(&output->events.destroy.listener_list));

	wlr_output_destroy_global(output);

	wl_list_remove(&output->display_destroy.link);

	// The backend is responsible for free-ing the list of modes

	struct wlr_output_cursor *cursor, *tmp_cursor;
	wl_list_for_each_safe(cursor, tmp_cursor, &output->cursors, link) {
		wlr_output_cursor_destroy(cursor);
	}

	struct wlr_output_layer *layer, *tmp_layer;
	wl_list_for_each_safe(layer, tmp_layer, &output->layers, link) {
		wlr_output_layer_destroy(layer);
	}

	wlr_swapchain_destroy(output->cursor_swapchain);
	wlr_buffer_unlock(output->cursor_front_buffer);
	wlr_color_transform_unref(output->color_transform);

	wlr_swapchain_destroy(output->swapchain);

	if (output->idle_frame != NULL) {
		wl_event_source_remove(output->idle_frame);
	}

	if (output->idle_done != NULL) {
		wl_event_source_remove(output->idle_done);
	}

	free(output->name);
	free(output->description);
	free(output->make);
	free(output->model);
	free(output->serial);
}

void wlr_output_destroy(struct wlr_output *output) {
	if (!output) {
		return;
	}

	if (output->impl && output->impl->destroy) {
		output->impl->destroy(output);
	} else {
		wlr_output_finish(output);
		free(output);
	}
}

void wlr_output_transformed_resolution(struct wlr_output *output,
		int *width, int *height) {
	if (output->transform % 2 == 0) {
		*width = output->width;
		*height = output->height;
	} else {
		*width = output->height;
		*height = output->width;
	}
}

void wlr_output_effective_resolution(struct wlr_output *output,
		int *width, int *height) {
	wlr_output_transformed_resolution(output, width, height);
	*width /= output->scale;
	*height /= output->scale;
}

struct wlr_output_mode *wlr_output_preferred_mode(struct wlr_output *output) {
	if (wl_list_empty(&output->modes)) {
		return NULL;
	}

	struct wlr_output_mode *mode;
	wl_list_for_each(mode, &output->modes, link) {
		if (mode->preferred) {
			return mode;
		}
	}

	// No preferred mode, choose the first one
	return wl_container_of(output->modes.next, mode, link);
}

void output_pending_resolution(struct wlr_output *output,
		const struct wlr_output_state *state, int *width, int *height) {
	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		switch (state->mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:
			*width = state->mode->width;
			*height = state->mode->height;
			return;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			*width = state->custom_mode.width;
			*height = state->custom_mode.height;
			return;
		}
		abort();
	} else {
		*width = output->width;
		*height = output->height;
	}
}

bool output_pending_enabled(struct wlr_output *output,
		const struct wlr_output_state *state) {
	if (state->committed & WLR_OUTPUT_STATE_ENABLED) {
		return state->enabled;
	}
	return output->enabled;
}

const struct wlr_output_image_description *output_pending_image_description(
		struct wlr_output *output, const struct wlr_output_state *state) {
	if (state->committed & WLR_OUTPUT_STATE_IMAGE_DESCRIPTION) {
		return state->image_description;
	}
	return output->image_description;
}

/**
 * Compare a struct wlr_output_state with the current state of a struct
 * wlr_output.
 *
 * Returns a bitfield of the unchanged fields.
 *
 * Some fields are not checked: damage always changes in-between frames, the
 * contents of the buffer might have changed, etc.
 */
static uint32_t output_compare_state(struct wlr_output *output,
		const struct wlr_output_state *state) {
	uint32_t fields = 0;
	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		bool unchanged = false;
		switch (state->mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:
			unchanged = output->current_mode == state->mode;
			break;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			unchanged = output->width == state->custom_mode.width &&
				output->height == state->custom_mode.height &&
				output->refresh == state->custom_mode.refresh;
			break;
		}
		if (unchanged) {
			fields |= WLR_OUTPUT_STATE_MODE;
		}
	}
	if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && output->enabled == state->enabled) {
		fields |= WLR_OUTPUT_STATE_ENABLED;
	}
	if ((state->committed & WLR_OUTPUT_STATE_SCALE) && output->scale == state->scale) {
		fields |= WLR_OUTPUT_STATE_SCALE;
	}
	if ((state->committed & WLR_OUTPUT_STATE_TRANSFORM) &&
			output->transform == state->transform) {
		fields |= WLR_OUTPUT_STATE_TRANSFORM;
	}
	if (state->committed & WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED) {
		bool enabled =
			output->adaptive_sync_status != WLR_OUTPUT_ADAPTIVE_SYNC_DISABLED;
		if (enabled == state->adaptive_sync_enabled) {
			fields |= WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED;
		}
	}
	if ((state->committed & WLR_OUTPUT_STATE_RENDER_FORMAT) &&
			output->render_format == state->render_format) {
		fields |= WLR_OUTPUT_STATE_RENDER_FORMAT;
	}
	if ((state->committed & WLR_OUTPUT_STATE_SUBPIXEL) &&
			output->subpixel == state->subpixel) {
		fields |= WLR_OUTPUT_STATE_SUBPIXEL;
	}
	if ((state->committed & WLR_OUTPUT_STATE_COLOR_TRANSFORM) &&
			output->color_transform == state->color_transform) {
		fields |= WLR_OUTPUT_STATE_COLOR_TRANSFORM;
	}
	return fields;
}

static bool output_basic_test(struct wlr_output *output,
		const struct wlr_output_state *state) {
	if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		struct wlr_fbox src_box;
		output_state_get_buffer_src_box(state, &src_box);

		// Source box must be contained within the buffer
		if (src_box.x < 0.0 || src_box.y < 0.0 ||
				src_box.x + src_box.width > state->buffer->width ||
				src_box.y + src_box.height > state->buffer->height) {
			wlr_log(WLR_ERROR, "Tried to commit with invalid buffer_src_box");
			return false;
		}

		// Source box must not be empty (but it can be smaller than 1 pixel,
		// some DRM devices support sub-pixel crops)
		if (wlr_fbox_empty(&src_box)) {
			wlr_log(WLR_ERROR, "Tried to commit with an empty buffer_src_box");
			return false;
		}

		// Destination box cannot be entirely off-screen (but it also doesn't
		// have to be entirely on-screen).  This also checks the dst box is
		// not empty.
		int pending_width, pending_height;
		output_pending_resolution(output, state, &pending_width, &pending_height);
		struct wlr_box output_box = {
			.width = pending_width,
			.height = pending_height
		};
		struct wlr_box dst_box;
		output_state_get_buffer_dst_box(state, &dst_box);
		if (!wlr_box_intersection(&output_box, &output_box, &dst_box)) {
			wlr_log(WLR_ERROR, "Primary buffer is entirely off-screen or 0-sized");
			return false;
		}
	} else {
		if (state->tearing_page_flip) {
			wlr_log(WLR_ERROR, "Tried to commit a tearing page flip without a buffer");
			return false;
		}
		if (state->committed & WLR_OUTPUT_STATE_WAIT_TIMELINE) {
			wlr_log(WLR_DEBUG, "Tried to set wait timeline without a buffer");
			return false;
		}
		if (state->committed & WLR_OUTPUT_STATE_SIGNAL_TIMELINE) {
			wlr_log(WLR_DEBUG, "Tried to set signal timeline without a buffer");
			return false;
		}
	}

	if (state->committed & WLR_OUTPUT_STATE_RENDER_FORMAT) {
		struct wlr_allocator *allocator = output->allocator;
		assert(allocator != NULL);

		const struct wlr_drm_format_set *display_formats =
			wlr_output_get_primary_formats(output, allocator->buffer_caps);
		struct wlr_drm_format format = {0};
		if (!output_pick_format(output, display_formats, &format, state->render_format)) {
			wlr_log(WLR_ERROR, "Failed to pick primary buffer format for output");
			return false;
		}

		wlr_drm_format_finish(&format);
	}

	bool enabled = output_pending_enabled(output, state);

	if (enabled && (state->committed & (WLR_OUTPUT_STATE_ENABLED |
			WLR_OUTPUT_STATE_MODE))) {
		int pending_width, pending_height;
		output_pending_resolution(output, state,
			&pending_width, &pending_height);
		if (pending_width == 0 || pending_height == 0) {
			wlr_log(WLR_DEBUG, "Tried to enable an output with a zero mode");
			return false;
		}
	}

	const struct {
		enum wlr_output_state_field field;
		const char *name;
	} needs_enabled[] = {
		{ WLR_OUTPUT_STATE_BUFFER, "buffer" },
		{ WLR_OUTPUT_STATE_MODE, "mode" },
		{ WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED, "adaptive sync" },
		{ WLR_OUTPUT_STATE_RENDER_FORMAT, "render format" },
		{ WLR_OUTPUT_STATE_SUBPIXEL, "subpixel" },
		{ WLR_OUTPUT_STATE_COLOR_TRANSFORM, "color transform" },
		{ WLR_OUTPUT_STATE_IMAGE_DESCRIPTION, "image description" },
	};
	if (!enabled) {
		for (size_t i = 0; i < sizeof(needs_enabled) / sizeof(needs_enabled[0]); i++) {
			if (state->committed & needs_enabled[i].field) {
				wlr_log(WLR_DEBUG, "Tried to set %s on a disabled output", needs_enabled[i].name);
				return false;
			}
		}
	}

	if (state->committed & WLR_OUTPUT_STATE_LAYERS) {
		if (state->layers_len != (size_t)wl_list_length(&output->layers)) {
			wlr_log(WLR_DEBUG, "All output layers must be specified in wlr_output_state.layers");
			return false;
		}

		for (size_t i = 0; i < state->layers_len; i++) {
			state->layers[i].accepted = false;
		}
	}

	if ((state->committed & (WLR_OUTPUT_STATE_WAIT_TIMELINE | WLR_OUTPUT_STATE_SIGNAL_TIMELINE)) &&
			!output->backend->features.timeline) {
		wlr_log(WLR_DEBUG, "Wait/signal timelines are not supported for this output");
		return false;
	}

	if ((state->committed & WLR_OUTPUT_STATE_IMAGE_DESCRIPTION) &&
			state->image_description != NULL) {
		if (!(output->supported_primaries & state->image_description->primaries)) {
			wlr_log(WLR_DEBUG, "Unsupported image description primaries");
			return false;
		}
		if (!(output->supported_transfer_functions & state->image_description->transfer_function)) {
			wlr_log(WLR_DEBUG, "Unsupported image description transfer function");
			return false;
		}
	}

	return true;
}

bool wlr_output_test_state(struct wlr_output *output,
		const struct wlr_output_state *state) {
	uint32_t unchanged = output_compare_state(output, state);

	// Create a shallow copy of the state with only the fields which have been
	// changed and potentially a new buffer.
	struct wlr_output_state copy = *state;
	copy.committed &= ~unchanged;

	if (!output_basic_test(output, &copy)) {
		return false;
	}
	if (!output->impl->test) {
		return true;
	}

	bool new_back_buffer = false;
	if (!output_ensure_buffer(output, &copy, &new_back_buffer)) {
		return false;
	}

	bool success = output->impl->test(output, &copy);
	if (new_back_buffer) {
		wlr_buffer_unlock(copy.buffer);
	}
	return success;
}

bool output_prepare_commit(struct wlr_output *output, const struct wlr_output_state *state) {
	if (!output_basic_test(output, state)) {
		wlr_log(WLR_ERROR, "Basic output test failed for %s", output->name);
		return false;
	}

	if ((state->committed & WLR_OUTPUT_STATE_BUFFER) &&
			output->idle_frame != NULL) {
		wl_event_source_remove(output->idle_frame);
		output->idle_frame = NULL;
	}

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	struct wlr_output_event_precommit pre_event = {
		.output = output,
		.when = now,
		.state = state,
	};
	wl_signal_emit_mutable(&output->events.precommit, &pre_event);

	return true;
}

void output_apply_commit(struct wlr_output *output, const struct wlr_output_state *state) {
	output->commit_seq++;

	if (output_pending_enabled(output, state)) {
		output->frame_pending = true;
		output->needs_frame = false;
	}

	output_apply_state(output, state);
}

void output_send_commit_event(struct wlr_output *output, const struct wlr_output_state *state) {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	struct wlr_output_event_commit event = {
		.output = output,
		.when = now,
		.state = state,
	};
	wl_signal_emit_mutable(&output->events.commit, &event);
}

bool wlr_output_commit_state(struct wlr_output *output,
		const struct wlr_output_state *state) {
	uint32_t unchanged = output_compare_state(output, state);

	// Create a shallow copy of the state with only the fields which have been
	// changed and potentially a new buffer.
	struct wlr_output_state pending = *state;
	pending.committed &= ~unchanged;

	if (!output_basic_test(output, &pending)) {
		wlr_log(WLR_ERROR, "Basic output test failed for %s", output->name);
		return false;
	}

	bool new_back_buffer = false;
	if (!output_ensure_buffer(output, &pending, &new_back_buffer)) {
		return false;
	}

	if (!output_prepare_commit(output, &pending)) {
		return false;
	}

	if (!output->impl->commit(output, &pending)) {
		if (new_back_buffer) {
			wlr_buffer_unlock(pending.buffer);
		}
		return false;
	}

	output_apply_commit(output, &pending);
	output_send_commit_event(output, &pending);

	if (new_back_buffer) {
		wlr_buffer_unlock(pending.buffer);
	}

	return true;
}

void wlr_output_send_frame(struct wlr_output *output) {
	output->frame_pending = false;
	if (output->enabled) {
		wl_signal_emit_mutable(&output->events.frame, output);
	}
}

static void schedule_frame_handle_idle_timer(void *data) {
	struct wlr_output *output = data;
	output->idle_frame = NULL;
	if (!output->frame_pending) {
		wlr_output_send_frame(output);
	}
}

void wlr_output_schedule_frame(struct wlr_output *output) {
	// Make sure the compositor commits a new frame. This is necessary to make
	// clients which ask for frame callbacks without submitting a new buffer
	// work.
	wlr_output_update_needs_frame(output);

	if (output->frame_pending || output->idle_frame != NULL) {
		return;
	}

	// We're using an idle timer here in case a buffer swap happens right after
	// this function is called
	output->idle_frame = wl_event_loop_add_idle(output->event_loop,
		schedule_frame_handle_idle_timer, output);
}

void wlr_output_send_present(struct wlr_output *output,
		struct wlr_output_event_present *event) {
	assert(event);
	event->output = output;

	if (event->presented && (event->when.tv_sec == 0 && event->when.tv_nsec == 0)) {
		if (clock_gettime(CLOCK_MONOTONIC, &event->when) != 0) {
			wlr_log_errno(WLR_ERROR, "failed to send output present event: "
				"failed to read clock");
			return;
		}
	}

	wl_signal_emit_mutable(&output->events.present, event);
}

struct deferred_present_event {
	struct wlr_output *output;
	struct wl_event_source *idle_source;
	struct wlr_output_event_present event;
	struct wl_listener output_destroy;
};

static void deferred_present_event_destroy(struct deferred_present_event *deferred) {
	wl_list_remove(&deferred->output_destroy.link);
	free(deferred);
}

static void deferred_present_event_handle_idle(void *data) {
	struct deferred_present_event *deferred = data;
	wlr_output_send_present(deferred->output, &deferred->event);
	deferred_present_event_destroy(deferred);
}

static void deferred_present_event_handle_output_destroy(struct wl_listener *listener, void *data) {
	struct deferred_present_event *deferred = wl_container_of(listener, deferred, output_destroy);
	wl_event_source_remove(deferred->idle_source);
	deferred_present_event_destroy(deferred);
}

void output_defer_present(struct wlr_output *output, struct wlr_output_event_present event) {
	struct deferred_present_event *deferred = calloc(1, sizeof(*deferred));
	if (!deferred) {
		return;
	}
	*deferred = (struct deferred_present_event){
		.output = output,
		.event = event,
	};
	deferred->output_destroy.notify = deferred_present_event_handle_output_destroy;
	wl_signal_add(&output->events.destroy, &deferred->output_destroy);

	deferred->idle_source = wl_event_loop_add_idle(output->event_loop,
		deferred_present_event_handle_idle, deferred);
}

void output_state_get_buffer_src_box(const struct wlr_output_state *state,
		struct wlr_fbox *out) {
	out->x = state->buffer_src_box.x;
	out->y = state->buffer_src_box.y;
	// If the source box is unset then default to the whole buffer.
	if (state->buffer_src_box.width == 0.0 &&
			state->buffer_src_box.height == 0.0) {
		out->width = (double)state->buffer->width;
		out->height = (double)state->buffer->height;
	} else {
		out->width = state->buffer_src_box.width;
		out->height = state->buffer_src_box.height;
	}
}

void output_state_get_buffer_dst_box(const struct wlr_output_state *state,
		struct wlr_box *out) {
	out->x = state->buffer_dst_box.x;
	out->y = state->buffer_dst_box.y;
	// If the dst box is unset then default to source crop size (which itself
	// defaults to the whole buffer size if unset)
	if (state->buffer_dst_box.width == 0 &&
			state->buffer_dst_box.height == 0) {
		struct wlr_fbox src_box;
		output_state_get_buffer_src_box(state, &src_box);
		out->width = (int)src_box.width;
		out->height = (int)src_box.height;
	} else {
		out->width = state->buffer_dst_box.width;
		out->height = state->buffer_dst_box.height;
	}
}

void wlr_output_send_request_state(struct wlr_output *output,
		const struct wlr_output_state *state) {
	uint32_t unchanged = output_compare_state(output, state);
	struct wlr_output_state copy = *state;
	copy.committed &= ~unchanged;
	if (copy.committed == 0) {
		return;
	}

	struct wlr_output_event_request_state event = {
		.output = output,
		.state = &copy,
	};
	wl_signal_emit_mutable(&output->events.request_state, &event);
}

size_t wlr_output_get_gamma_size(struct wlr_output *output) {
	if (!output->impl->get_gamma_size) {
		return 0;
	}
	return output->impl->get_gamma_size(output);
}

void wlr_output_update_needs_frame(struct wlr_output *output) {
	if (output->needs_frame) {
		return;
	}
	output->needs_frame = true;
	wl_signal_emit_mutable(&output->events.needs_frame, output);
}

const struct wlr_drm_format_set *wlr_output_get_primary_formats(
		struct wlr_output *output, uint32_t buffer_caps) {
	if (!output->impl->get_primary_formats) {
		return NULL;
	}

	const struct wlr_drm_format_set *formats =
		output->impl->get_primary_formats(output, buffer_caps);
	if (formats == NULL) {
		wlr_log(WLR_ERROR, "Failed to get primary display formats");

		static const struct wlr_drm_format_set empty_format_set = {0};
		return &empty_format_set;
	}

	return formats;
}

bool wlr_output_is_direct_scanout_allowed(struct wlr_output *output) {
	if (output->attach_render_locks > 0) {
		wlr_log(WLR_DEBUG, "Direct scan-out disabled by lock");
		return false;
	}

	// If the output has at least one software cursor, reject direct scan-out
	struct wlr_output_cursor *cursor;
	wl_list_for_each(cursor, &output->cursors, link) {
		if (cursor->enabled && cursor->visible &&
				cursor != output->hardware_cursor) {
			wlr_log(WLR_DEBUG,
				"Direct scan-out disabled by software cursor");
			return false;
		}
	}

	return true;
}
