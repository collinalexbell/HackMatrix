#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <drm_fourcc.h>
#include <wayland-client-protocol.h>

#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output_layer.h>
#include <wlr/util/log.h>

#include "backend/wayland.h"
#include "render/pixel_format.h"
#include "types/wlr_output.h"

#include "linux-dmabuf-v1-client-protocol.h"
#include "linux-drm-syncobj-v1-client-protocol.h"
#include "presentation-time-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "xdg-activation-v1-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

static const uint32_t SUPPORTED_OUTPUT_STATE =
	WLR_OUTPUT_STATE_BACKEND_OPTIONAL |
	WLR_OUTPUT_STATE_BUFFER |
	WLR_OUTPUT_STATE_ENABLED |
	WLR_OUTPUT_STATE_MODE |
	WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED |
	WLR_OUTPUT_STATE_WAIT_TIMELINE |
	WLR_OUTPUT_STATE_SIGNAL_TIMELINE;

static size_t last_output_num = 0;

static const char *surface_tag = "wlr_wl_output";

static struct wlr_wl_output *get_wl_output_from_output(
		struct wlr_output *wlr_output) {
	assert(wlr_output_is_wl(wlr_output));
	struct wlr_wl_output *output = wl_container_of(wlr_output, output, wlr_output);
	return output;
}

struct wlr_wl_output *get_wl_output_from_surface(struct wlr_wl_backend *wl,
		struct wl_surface *surface) {
	if (wl_proxy_get_tag((struct wl_proxy *)surface) != &surface_tag) {
		return NULL;
	}
	struct wlr_wl_output *output = wl_surface_get_user_data(surface);
	assert(output != NULL);
	if (output->backend != wl) {
		return NULL;
	}
	return output;
}

static void surface_frame_callback(void *data, struct wl_callback *cb,
		uint32_t time) {
	struct wlr_wl_output *output = data;

	if (cb == NULL) {
		return;
	}

	assert(output->frame_callback == cb);
	wl_callback_destroy(cb);
	output->frame_callback = NULL;

	wlr_output_send_frame(&output->wlr_output);
}

static const struct wl_callback_listener frame_listener = {
	.done = surface_frame_callback
};

static void presentation_feedback_destroy(
		struct wlr_wl_presentation_feedback *feedback) {
	wl_list_remove(&feedback->link);
	wp_presentation_feedback_destroy(feedback->feedback);
	free(feedback);
}

static void presentation_feedback_handle_sync_output(void *data,
		struct wp_presentation_feedback *feedback, struct wl_output *output) {
	// This space is intentionally left blank
}

static void presentation_feedback_handle_presented(void *data,
		struct wp_presentation_feedback *wp_feedback, uint32_t tv_sec_hi,
		uint32_t tv_sec_lo, uint32_t tv_nsec, uint32_t refresh_ns,
		uint32_t seq_hi, uint32_t seq_lo, uint32_t flags) {
	struct wlr_wl_presentation_feedback *feedback = data;

	struct wlr_output_event_present event = {
		.commit_seq = feedback->commit_seq,
		.presented = true,
		.when = {
			.tv_sec = ((uint64_t)tv_sec_hi << 32) | tv_sec_lo,
			.tv_nsec = tv_nsec,
		},
		.seq = ((uint64_t)seq_hi << 32) | seq_lo,
		.refresh = refresh_ns,
		.flags = flags,
	};
	wlr_output_send_present(&feedback->output->wlr_output, &event);

	presentation_feedback_destroy(feedback);
}

static void presentation_feedback_handle_discarded(void *data,
		struct wp_presentation_feedback *wp_feedback) {
	struct wlr_wl_presentation_feedback *feedback = data;

	struct wlr_output_event_present event = {
		.commit_seq = feedback->commit_seq,
		.presented = false,
	};
	wlr_output_send_present(&feedback->output->wlr_output, &event);

	presentation_feedback_destroy(feedback);
}

static const struct wp_presentation_feedback_listener
		presentation_feedback_listener = {
	.sync_output = presentation_feedback_handle_sync_output,
	.presented = presentation_feedback_handle_presented,
	.discarded = presentation_feedback_handle_discarded,
};

static void buffer_remove_drm_syncobj_waiter(struct wlr_wl_buffer *buffer) {
	wlr_drm_syncobj_timeline_waiter_finish(&buffer->drm_syncobj_waiter);
	buffer->has_drm_syncobj_waiter = false;
}

void destroy_wl_buffer(struct wlr_wl_buffer *buffer) {
	if (buffer == NULL) {
		return;
	}
	wl_list_remove(&buffer->buffer_destroy.link);
	wl_list_remove(&buffer->link);
	wl_buffer_destroy(buffer->wl_buffer);
	if (buffer->has_drm_syncobj_waiter) {
		buffer_remove_drm_syncobj_waiter(buffer);
	}
	if (!buffer->released) {
		wlr_buffer_unlock(buffer->buffer);
	}
	wlr_drm_syncobj_timeline_unref(buffer->fallback_signal_timeline);
	free(buffer);
}

static void buffer_release(struct wlr_wl_buffer *buffer) {
	if (buffer->released) {
		return;
	}
	buffer->released = true;
	wlr_buffer_unlock(buffer->buffer); // might free buffer
}

static void buffer_handle_release(void *data, struct wl_buffer *wl_buffer) {
	struct wlr_wl_buffer *buffer = data;
	if (buffer->has_drm_syncobj_waiter) {
		return;
	}
	buffer_release(buffer);
}

static const struct wl_buffer_listener buffer_listener = {
	.release = buffer_handle_release,
};

static void buffer_handle_drm_syncobj_ready(struct wlr_drm_syncobj_timeline_waiter *waiter) {
	struct wlr_wl_buffer *buffer = wl_container_of(waiter, buffer, drm_syncobj_waiter);
	buffer_remove_drm_syncobj_waiter(buffer);
	buffer_release(buffer);
}

static void buffer_handle_buffer_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_wl_buffer *buffer =
		wl_container_of(listener, buffer, buffer_destroy);
	destroy_wl_buffer(buffer);
}

static bool test_buffer(struct wlr_wl_backend *wl,
		struct wlr_buffer *wlr_buffer) {
	struct wlr_dmabuf_attributes dmabuf;
	struct wlr_shm_attributes shm;
	if (wlr_buffer_get_dmabuf(wlr_buffer, &dmabuf)) {
		return wlr_drm_format_set_has(&wl->linux_dmabuf_v1_formats,
			dmabuf.format, dmabuf.modifier);
	} else if (wlr_buffer_get_shm(wlr_buffer, &shm)) {
		return wlr_drm_format_set_has(&wl->shm_formats, shm.format,
			DRM_FORMAT_MOD_INVALID);
	} else {
		return false;
	}
}

struct dmabuf_listener_data {
	struct wl_buffer *wl_buffer;
	bool done;
};

static void dmabuf_handle_created(void *data_, struct zwp_linux_buffer_params_v1 *params,
		struct wl_buffer *buffer) {
	struct dmabuf_listener_data *data = data_;
	data->wl_buffer = buffer;
	data->done = true;
	wlr_log(WLR_DEBUG, "DMA-BUF imported into parent Wayland compositor");
}

static void dmabuf_handle_failed(void *data_, struct zwp_linux_buffer_params_v1 *params) {
	struct dmabuf_listener_data *data = data_;
	data->done = true;
	wlr_log(WLR_ERROR, "Failed to import DMA-BUF into parent Wayland compositor");
}

static const struct zwp_linux_buffer_params_v1_listener dmabuf_listener = {
	.created = dmabuf_handle_created,
	.failed = dmabuf_handle_failed,
};

static struct wl_buffer *import_dmabuf(struct wlr_wl_backend *wl,
		struct wlr_dmabuf_attributes *dmabuf) {
	uint32_t modifier_hi = dmabuf->modifier >> 32;
	uint32_t modifier_lo = (uint32_t)dmabuf->modifier;
	struct zwp_linux_buffer_params_v1 *params =
		zwp_linux_dmabuf_v1_create_params(wl->zwp_linux_dmabuf_v1);
	for (int i = 0; i < dmabuf->n_planes; i++) {
		zwp_linux_buffer_params_v1_add(params, dmabuf->fd[i], i,
			dmabuf->offset[i], dmabuf->stride[i], modifier_hi, modifier_lo);
	}

	struct dmabuf_listener_data data = {0};
	zwp_linux_buffer_params_v1_add_listener(params, &dmabuf_listener, &data);
	zwp_linux_buffer_params_v1_create(params, dmabuf->width, dmabuf->height, dmabuf->format, 0);

	struct wl_event_queue *display_queue =
		wl_proxy_get_queue((struct wl_proxy *)wl->remote_display);
	wl_proxy_set_queue((struct wl_proxy *)params, wl->busy_loop_queue);

	while (!data.done) {
		if (wl_display_dispatch_queue(wl->remote_display, wl->busy_loop_queue) < 0) {
			wlr_log(WLR_ERROR, "wl_display_dispatch_queue() failed");
			break;
		}
	}

	struct wl_buffer *buffer = data.wl_buffer;
	if (buffer != NULL) {
		wl_proxy_set_queue((struct wl_proxy *)buffer, display_queue);
	}

	zwp_linux_buffer_params_v1_destroy(params);
	return buffer;
}

static struct wl_buffer *import_shm(struct wlr_wl_backend *wl,
		struct wlr_shm_attributes *shm) {
	enum wl_shm_format wl_shm_format = convert_drm_format_to_wl_shm(shm->format);
	uint32_t size = shm->stride * shm->height;
	struct wl_shm_pool *pool = wl_shm_create_pool(wl->shm, shm->fd, shm->offset + size);
	if (pool == NULL) {
		return NULL;
	}
	struct wl_buffer *wl_buffer = wl_shm_pool_create_buffer(pool, shm->offset,
		shm->width, shm->height, shm->stride, wl_shm_format);
	wl_shm_pool_destroy(pool);
	return wl_buffer;
}

static struct wlr_wl_buffer *create_wl_buffer(struct wlr_wl_backend *wl,
		struct wlr_buffer *wlr_buffer) {
	if (!test_buffer(wl, wlr_buffer)) {
		return NULL;
	}

	struct wlr_dmabuf_attributes dmabuf;
	struct wlr_shm_attributes shm;
	struct wl_buffer *wl_buffer;
	if (wlr_buffer_get_dmabuf(wlr_buffer, &dmabuf)) {
		wl_buffer = import_dmabuf(wl, &dmabuf);
	} else if (wlr_buffer_get_shm(wlr_buffer, &shm)) {
		wl_buffer = import_shm(wl, &shm);
	} else {
		return NULL;
	}
	if (wl_buffer == NULL) {
		return NULL;
	}

	struct wlr_wl_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		wl_buffer_destroy(wl_buffer);
		return NULL;
	}
	buffer->wl_buffer = wl_buffer;
	buffer->buffer = wlr_buffer_lock(wlr_buffer);
	wl_list_insert(&wl->buffers, &buffer->link);

	wl_buffer_add_listener(wl_buffer, &buffer_listener, buffer);

	buffer->buffer_destroy.notify = buffer_handle_buffer_destroy;
	wl_signal_add(&wlr_buffer->events.destroy, &buffer->buffer_destroy);

	return buffer;
}

static struct wlr_wl_buffer *get_or_create_wl_buffer(struct wlr_wl_backend *wl,
		struct wlr_buffer *wlr_buffer) {
	struct wlr_wl_buffer *buffer;
	wl_list_for_each(buffer, &wl->buffers, link) {
		// We can only re-use a wlr_wl_buffer if the parent compositor has
		// released it, because wl_buffer.release is per-wl_buffer, not per
		// wl_surface.commit.
		if (buffer->buffer == wlr_buffer && buffer->released) {
			buffer->released = false;
			wlr_buffer_lock(buffer->buffer);
			return buffer;
		}
	}

	return create_wl_buffer(wl, wlr_buffer);
}

void destroy_wl_drm_syncobj_timeline(struct wlr_wl_drm_syncobj_timeline *timeline) {
	wp_linux_drm_syncobj_timeline_v1_destroy(timeline->wl);
	wlr_addon_finish(&timeline->addon);
	wl_list_remove(&timeline->link);
	free(timeline);
}

static void drm_syncobj_timeline_addon_destroy(struct wlr_addon *addon) {
	struct wlr_wl_drm_syncobj_timeline *timeline = wl_container_of(addon, timeline, addon);
	destroy_wl_drm_syncobj_timeline(timeline);
}

static const struct wlr_addon_interface drm_syncobj_timeline_addon_impl = {
	.name = "wlr_wl_drm_syncobj_timeline",
	.destroy = drm_syncobj_timeline_addon_destroy,
};

static struct wlr_wl_drm_syncobj_timeline *get_or_create_drm_syncobj_timeline(
		struct wlr_wl_backend *wl, struct wlr_drm_syncobj_timeline *wlr_timeline) {
	struct wlr_addon *addon =
		wlr_addon_find(&wlr_timeline->addons, wl, &drm_syncobj_timeline_addon_impl);
	if (addon != NULL) {
		struct wlr_wl_drm_syncobj_timeline *timeline = wl_container_of(addon, timeline, addon);
		return timeline;
	}

	struct wlr_wl_drm_syncobj_timeline *timeline = calloc(1, sizeof(*timeline));
	if (timeline == NULL) {
		return NULL;
	}

	timeline->base = wlr_timeline;

	int fd = wlr_drm_syncobj_timeline_export(wlr_timeline);
	if (fd < 0) {
		free(timeline);
		return NULL;
	}

	timeline->wl = wp_linux_drm_syncobj_manager_v1_import_timeline(wl->drm_syncobj_manager_v1, fd);
	close(fd);
	if (timeline->wl == NULL) {
		free(timeline);
		return NULL;
	}

	wlr_addon_init(&timeline->addon, &wlr_timeline->addons, wl, &drm_syncobj_timeline_addon_impl);
	wl_list_insert(&wl->drm_syncobj_timelines, &timeline->link);

	return timeline;
}

static bool update_title(struct wlr_wl_output *output, const char *title) {
	struct wlr_output *wlr_output = &output->wlr_output;

	char default_title[64];
	if (title == NULL) {
		snprintf(default_title, sizeof(default_title), "wlroots - %s", wlr_output->name);
		title = default_title;
	}

	char *wl_title = strdup(title);
	if (wl_title == NULL) {
		return false;
	}

	free(output->title);
	output->title = wl_title;
	return true;
}

static bool update_app_id(struct wlr_wl_output *output, const char *app_id) {
	if (app_id == NULL) {
		app_id = "wlroots";
	}

	char *wl_app_id = strdup(app_id);
	if (wl_app_id == NULL) {
		return false;
	}

	free(output->app_id);
	output->app_id = wl_app_id;
	return true;
}

static bool output_test(struct wlr_output *wlr_output,
		const struct wlr_output_state *state) {
	struct wlr_wl_output *output =
		get_wl_output_from_output(wlr_output);

	if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		// If the size doesn't match, reject buffer (scaling is not currently
		// supported but could be implemented with viewporter)
		int pending_width, pending_height;
		output_pending_resolution(wlr_output, state,
			&pending_width, &pending_height);
		if (state->buffer->width != pending_width ||
				state->buffer->height != pending_height) {
			wlr_log(WLR_DEBUG, "Primary buffer size mismatch");
			return false;
		}
		// Source crop is also not currently supported
		struct wlr_fbox src_box;
		output_state_get_buffer_src_box(state, &src_box);
		if (src_box.x != 0.0 || src_box.y != 0.0 ||
				src_box.width != (double)state->buffer->width ||
				src_box.height != (double)state->buffer->height) {
			wlr_log(WLR_DEBUG, "Source crop not supported in wayland output");
			return false;
		}
	}

	uint32_t unsupported = state->committed & ~SUPPORTED_OUTPUT_STATE;
	if (unsupported != 0) {
		wlr_log(WLR_DEBUG, "Unsupported output state fields: 0x%"PRIx32,
			unsupported);
		return false;
	}

	// Adaptive sync is effectively always enabled when using the Wayland
	// backend. This is not something we have control over, so we set the state
	// to enabled on creating the output and never allow changing it.
	assert(wlr_output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED);
	if (state->committed & WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED) {
		if (!state->adaptive_sync_enabled) {
			wlr_log(WLR_DEBUG, "Disabling adaptive sync is not supported");
			return false;
		}
	}

	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		assert(state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM);

		if (state->custom_mode.refresh != 0) {
			wlr_log(WLR_DEBUG, "Refresh rates are not supported");
			return false;
		}
	}

	if ((state->committed & WLR_OUTPUT_STATE_BUFFER) &&
			!test_buffer(output->backend, state->buffer)) {
		wlr_log(WLR_DEBUG, "Unsupported buffer format");
		return false;
	}

	if ((state->committed & WLR_OUTPUT_STATE_SIGNAL_TIMELINE) &&
			!(state->committed & WLR_OUTPUT_STATE_WAIT_TIMELINE)) {
		wlr_log(WLR_DEBUG, "Signal timeline requires a wait timeline");
		return false;
	}

	if ((state->committed & WLR_OUTPUT_STATE_WAIT_TIMELINE) ||
			(state->committed & WLR_OUTPUT_STATE_SIGNAL_TIMELINE)) {
		struct wlr_dmabuf_attributes dmabuf;
		if (!wlr_buffer_get_dmabuf(state->buffer, &dmabuf)) {
			wlr_log(WLR_DEBUG, "Wait/signal timelines require DMA-BUFs");
			return false;
		}
	}

	if (state->committed & WLR_OUTPUT_STATE_LAYERS) {
		// If we can't use a sub-surface for a layer, then we can't use a
		// sub-surface for any layer underneath
		bool supported = output->backend->subcompositor != NULL;
		for (ssize_t i = state->layers_len - 1; i >= 0; i--) {
			struct wlr_output_layer_state *layer_state = &state->layers[i];
			if (layer_state->buffer != NULL) {
				int x = layer_state->dst_box.x;
				int y = layer_state->dst_box.y;
				int width = layer_state->dst_box.width;
				int height = layer_state->dst_box.height;
				bool needs_viewport = width != layer_state->buffer->width ||
					height != layer_state->buffer->height;
				if (!wlr_fbox_empty(&layer_state->src_box)) {
					needs_viewport = needs_viewport ||
						layer_state->src_box.x != 0 ||
						layer_state->src_box.y != 0 ||
						layer_state->src_box.width != width ||
						layer_state->src_box.height != height;
				}
				if (x < 0 || y < 0 ||
						x + width > wlr_output->width ||
						y + height > wlr_output->height ||
						(output->backend->viewporter == NULL && needs_viewport)) {
					supported = false;
				}
				supported = supported &&
					test_buffer(output->backend, layer_state->buffer);
			}
			layer_state->accepted = supported;
		}
	}

	return true;
}

static void output_layer_handle_addon_destroy(struct wlr_addon *addon) {
	struct wlr_wl_output_layer *layer = wl_container_of(addon, layer, addon);

	wlr_addon_finish(&layer->addon);
	if (layer->viewport != NULL) {
		wp_viewport_destroy(layer->viewport);
	}
	wl_subsurface_destroy(layer->subsurface);
	wl_surface_destroy(layer->surface);
	free(layer);
}

static const struct wlr_addon_interface output_layer_addon_impl = {
	.name = "wlr_wl_output_layer",
	.destroy = output_layer_handle_addon_destroy,
};

static struct wlr_wl_output_layer *get_or_create_output_layer(
		struct wlr_wl_output *output, struct wlr_output_layer *wlr_layer) {
	assert(output->backend->subcompositor != NULL);

	struct wlr_wl_output_layer *layer;
	struct wlr_addon *addon = wlr_addon_find(&wlr_layer->addons, output,
		&output_layer_addon_impl);
	if (addon != NULL) {
		layer = wl_container_of(addon, layer, addon);
		return layer;
	}

	layer = calloc(1, sizeof(*layer));
	if (layer == NULL) {
		return NULL;
	}

	wlr_addon_init(&layer->addon, &wlr_layer->addons, output,
		&output_layer_addon_impl);

	layer->surface = wl_compositor_create_surface(output->backend->compositor);
	layer->subsurface = wl_subcompositor_get_subsurface(
		output->backend->subcompositor, layer->surface, output->surface);

	// Set an empty input region so that input events are handled by the main
	// surface
	struct wl_region *region = wl_compositor_create_region(output->backend->compositor);
	wl_surface_set_input_region(layer->surface, region);
	wl_region_destroy(region);

	if (output->backend->viewporter != NULL) {
		layer->viewport = wp_viewporter_get_viewport(output->backend->viewporter, layer->surface);
	}

	return layer;
}

static bool has_layers_order_changed(struct wlr_wl_output *output,
		struct wlr_output_layer_state *layers, size_t layers_len) {
	// output_basic_check() ensures that layers_len equals the number of
	// registered output layers
	size_t i = 0;
	struct wlr_output_layer *layer;
	wl_list_for_each(layer, &output->wlr_output.layers, link) {
		assert(i < layers_len);
		const struct wlr_output_layer_state *layer_state = &layers[i];
		if (layer_state->layer != layer) {
			return true;
		}
		i++;
	}
	assert(i == layers_len);
	return false;
}

static void output_layer_unmap(struct wlr_wl_output_layer *layer) {
	if (!layer->mapped) {
		return;
	}

	wl_surface_attach(layer->surface, NULL, 0, 0);
	wl_surface_commit(layer->surface);
	layer->mapped = false;
}

static void damage_surface(struct wl_surface *surface,
		const pixman_region32_t *damage) {
	if (damage == NULL) {
		wl_surface_damage_buffer(surface,
			0, 0, INT32_MAX, INT32_MAX);
		return;
	}

	int rects_len;
	const pixman_box32_t *rects = pixman_region32_rectangles(damage, &rects_len);
	for (int i = 0; i < rects_len; i++) {
		const pixman_box32_t *r = &rects[i];
		wl_surface_damage_buffer(surface, r->x1, r->y1,
			r->x2 - r->x1, r->y2 - r->y1);
	}
}

static bool output_layer_commit(struct wlr_wl_output *output,
		struct wlr_wl_output_layer *layer,
		const struct wlr_output_layer_state *state) {
	if (state->layer->dst_box.x != state->dst_box.x ||
			state->layer->dst_box.y != state->dst_box.y) {
		wl_subsurface_set_position(layer->subsurface, state->dst_box.x, state->dst_box.y);
	}

	if (state->buffer == NULL) {
		output_layer_unmap(layer);
		return true;
	}

	struct wlr_wl_buffer *buffer =
		get_or_create_wl_buffer(output->backend, state->buffer);
	if (buffer == NULL) {
		return false;
	}

	if (layer->viewport != NULL &&
			(state->layer->dst_box.width != state->dst_box.width ||
			state->layer->dst_box.height != state->dst_box.height)) {
		wp_viewport_set_destination(layer->viewport, state->dst_box.width, state->dst_box.height);
	}
	if (layer->viewport != NULL && !wlr_fbox_equal(&state->layer->src_box, &state->src_box)) {
		struct wlr_fbox src_box = state->src_box;
		if (wlr_fbox_empty(&src_box)) {
			// -1 resets the box
			src_box = (struct wlr_fbox){
				.x = -1,
				.y = -1,
				.width = -1,
				.height = -1,
			};
		}
		wp_viewport_set_source(layer->viewport,
			wl_fixed_from_double(src_box.x),
			wl_fixed_from_double(src_box.y),
			wl_fixed_from_double(src_box.width),
			wl_fixed_from_double(src_box.height));
	}

	wl_surface_attach(layer->surface, buffer->wl_buffer, 0, 0);
	damage_surface(layer->surface, state->damage);
	wl_surface_commit(layer->surface);
	layer->mapped = true;
	return true;
}

static bool commit_layers(struct wlr_wl_output *output,
		struct wlr_output_layer_state *layers, size_t layers_len) {
	if (output->backend->subcompositor == NULL) {
		return true;
	}

	bool reordered = has_layers_order_changed(output, layers, layers_len);

	struct wlr_wl_output_layer *prev_layer = NULL;
	for (size_t i = 0; i < layers_len; i++) {
		struct wlr_wl_output_layer *layer =
			get_or_create_output_layer(output, layers[i].layer);
		if (layer == NULL) {
			return false;
		}

		if (!layers[i].accepted) {
			output_layer_unmap(layer);
			continue;
		}

		if (prev_layer != NULL && reordered) {
			wl_subsurface_place_above(layer->subsurface,
				prev_layer->surface);
		}

		if (!output_layer_commit(output, layer, &layers[i])) {
			return false;
		}

		prev_layer = layer;
	}

	return true;
}

static void unmap_callback_handle_done(void *data, struct wl_callback *callback,
		uint32_t cb_data) {
	struct wlr_wl_output *output = data;
	output->unmap_callback = NULL;
	wl_callback_destroy(callback);
}

static const struct wl_callback_listener unmap_callback_listener = {
	.done = unmap_callback_handle_done,
};

static bool output_commit(struct wlr_output *wlr_output, const struct wlr_output_state *state) {
	struct wlr_wl_output *output = get_wl_output_from_output(wlr_output);

	if (!output_test(wlr_output, state)) {
		return false;
	}

	struct wlr_wl_backend *wl = output->backend;

	bool pending_enabled = output_pending_enabled(wlr_output, state);

	if (wlr_output->enabled && !pending_enabled) {
		if (output->own_surface) {
			output->unmap_callback = wl_display_sync(output->backend->remote_display);
			if (output->unmap_callback == NULL) {
				return false;
			}
			wl_callback_add_listener(output->unmap_callback, &unmap_callback_listener, output);
		}

		wl_surface_attach(output->surface, NULL, 0, 0);
		wl_surface_commit(output->surface);

		output->initialized = false;
		output->configured = false;
		output->has_configure_serial = false;
		output->requested_width = output->requested_height = 0;
	} else if (output->own_surface && pending_enabled && !output->initialized) {
		xdg_toplevel_set_title(output->xdg_toplevel, output->title);
		xdg_toplevel_set_app_id(output->xdg_toplevel, output->app_id);
		wl_surface_commit(output->surface);
		output->initialized = true;

		struct wl_event_queue *display_queue =
			wl_proxy_get_queue((struct wl_proxy *)wl->remote_display);
		wl_proxy_set_queue((struct wl_proxy *)output->xdg_surface, wl->busy_loop_queue);
		wl_proxy_set_queue((struct wl_proxy *)output->xdg_toplevel, wl->busy_loop_queue);

		wl_display_flush(wl->remote_display);
		while (!output->configured) {
			if (wl_display_dispatch_queue(wl->remote_display, wl->busy_loop_queue) == -1) {
				wlr_log(WLR_ERROR, "wl_display_dispatch_queue() failed");
				break;
			}
		}

		wl_proxy_set_queue((struct wl_proxy *)output->xdg_surface, display_queue);
		wl_proxy_set_queue((struct wl_proxy *)output->xdg_toplevel, display_queue);

		if (!output->configured) {
			return false;
		}
	}

	struct wlr_wl_buffer *buffer = NULL;
	if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		const pixman_region32_t *damage = NULL;
		if (state->committed & WLR_OUTPUT_STATE_DAMAGE) {
			damage = &state->damage;
		}

		struct wlr_buffer *wlr_buffer = state->buffer;
		buffer = get_or_create_wl_buffer(wl, wlr_buffer);
		if (buffer == NULL) {
			return false;
		}

		wl_surface_attach(output->surface, buffer->wl_buffer, 0, 0);
		damage_surface(output->surface, damage);
	}

	if (state->committed & WLR_OUTPUT_STATE_WAIT_TIMELINE) {
		struct wlr_wl_drm_syncobj_timeline *wait_timeline =
			get_or_create_drm_syncobj_timeline(wl, state->wait_timeline);

		struct wlr_wl_drm_syncobj_timeline *signal_timeline;
		uint64_t signal_point;
		if (state->committed & WLR_OUTPUT_STATE_SIGNAL_TIMELINE) {
			signal_timeline = get_or_create_drm_syncobj_timeline(wl, state->signal_timeline);
			signal_point = state->signal_point;
		} else {
			if (buffer->fallback_signal_timeline == NULL) {
				buffer->fallback_signal_timeline =
					wlr_drm_syncobj_timeline_create(wl->drm_fd);
				if (buffer->fallback_signal_timeline == NULL) {
					return false;
				}
			}
			signal_timeline =
				get_or_create_drm_syncobj_timeline(wl, buffer->fallback_signal_timeline);
			signal_point = ++buffer->fallback_signal_point;
		}

		if (wait_timeline == NULL || signal_timeline == NULL) {
			return false;
		}

		if (output->drm_syncobj_surface_v1 == NULL) {
			output->drm_syncobj_surface_v1 = wp_linux_drm_syncobj_manager_v1_get_surface(
				wl->drm_syncobj_manager_v1, output->surface);
			if (output->drm_syncobj_surface_v1 == NULL) {
				return false;
			}
		}

		uint32_t wait_point_hi = state->wait_point >> 32;
		uint32_t wait_point_lo = state->wait_point & UINT32_MAX;
		uint32_t signal_point_hi = signal_point >> 32;
		uint32_t signal_point_lo = signal_point & UINT32_MAX;

		wp_linux_drm_syncobj_surface_v1_set_acquire_point(output->drm_syncobj_surface_v1,
			wait_timeline->wl, wait_point_hi, wait_point_lo);
		wp_linux_drm_syncobj_surface_v1_set_release_point(output->drm_syncobj_surface_v1,
			signal_timeline->wl, signal_point_hi, signal_point_lo);

		if (!wlr_drm_syncobj_timeline_waiter_init(&buffer->drm_syncobj_waiter,
				signal_timeline->base, signal_point, 0, wl->event_loop,
				buffer_handle_drm_syncobj_ready)) {
			return false;
		}
		buffer->has_drm_syncobj_waiter = true;
	} else {
		if (output->drm_syncobj_surface_v1 != NULL) {
			wp_linux_drm_syncobj_surface_v1_destroy(output->drm_syncobj_surface_v1);
			output->drm_syncobj_surface_v1 = NULL;
		}
	}

	if ((state->committed & WLR_OUTPUT_STATE_LAYERS) &&
			!commit_layers(output, state->layers, state->layers_len)) {
		return false;
	}

	if (pending_enabled) {
		if (output->frame_callback != NULL) {
			wl_callback_destroy(output->frame_callback);
		}
		output->frame_callback = wl_surface_frame(output->surface);
		wl_callback_add_listener(output->frame_callback, &frame_listener, output);

		struct wp_presentation_feedback *wp_feedback = NULL;
		if (wl->presentation != NULL) {
			wp_feedback = wp_presentation_feedback(wl->presentation, output->surface);
		}

		if (output->has_configure_serial) {
			xdg_surface_ack_configure(output->xdg_surface, output->configure_serial);
			output->has_configure_serial = false;
		}

		wl_surface_commit(output->surface);

		if (wp_feedback != NULL) {
			struct wlr_wl_presentation_feedback *feedback =
				calloc(1, sizeof(*feedback));
			if (feedback == NULL) {
				wp_presentation_feedback_destroy(wp_feedback);
				return false;
			}
			feedback->output = output;
			feedback->feedback = wp_feedback;
			feedback->commit_seq = output->wlr_output.commit_seq + 1;
			wl_list_insert(&output->presentation_feedbacks, &feedback->link);

			wp_presentation_feedback_add_listener(wp_feedback,
				&presentation_feedback_listener, feedback);
		} else {
			struct wlr_output_event_present present_event = {
				.commit_seq = wlr_output->commit_seq + 1,
				.presented = true,
			};
			output_defer_present(wlr_output, present_event);
		}
	}

	wl_display_flush(wl->remote_display);

	return true;
}

static bool output_set_cursor(struct wlr_output *wlr_output,
		struct wlr_buffer *wlr_buffer, int hotspot_x, int hotspot_y) {
	struct wlr_wl_output *output = get_wl_output_from_output(wlr_output);
	struct wlr_wl_backend *backend = output->backend;

	output->cursor.hotspot_x = hotspot_x;
	output->cursor.hotspot_y = hotspot_y;

	if (output->cursor.surface == NULL) {
		output->cursor.surface =
			wl_compositor_create_surface(backend->compositor);
	}
	struct wl_surface *surface = output->cursor.surface;

	if (wlr_buffer != NULL) {
		struct wlr_wl_buffer *buffer =
			get_or_create_wl_buffer(output->backend, wlr_buffer);
		if (buffer == NULL) {
			return false;
		}

		wl_surface_attach(surface, buffer->wl_buffer, 0, 0);
		wl_surface_damage_buffer(surface, 0, 0, INT32_MAX, INT32_MAX);
		wl_surface_commit(surface);
	} else {
		wl_surface_attach(surface, NULL, 0, 0);
		wl_surface_commit(surface);
	}

	update_wl_output_cursor(output);
	wl_display_flush(backend->remote_display);
	return true;
}

static const struct wlr_drm_format_set *output_get_formats(
		struct wlr_output *wlr_output, uint32_t buffer_caps) {
	struct wlr_wl_output *output = get_wl_output_from_output(wlr_output);
	if (buffer_caps & WLR_BUFFER_CAP_DMABUF) {
		return &output->backend->linux_dmabuf_v1_formats;
	} else if (buffer_caps & WLR_BUFFER_CAP_SHM) {
		return &output->backend->shm_formats;
	}
	return NULL;
}

static void output_destroy(struct wlr_output *wlr_output) {
	struct wlr_wl_output *output = get_wl_output_from_output(wlr_output);
	if (output == NULL) {
		return;
	}

	wlr_output_finish(wlr_output);

	wl_list_remove(&output->link);

	if (output->cursor.surface) {
		wl_surface_destroy(output->cursor.surface);
	}

	if (output->frame_callback) {
		wl_callback_destroy(output->frame_callback);
	}

	struct wlr_wl_presentation_feedback *feedback, *feedback_tmp;
	wl_list_for_each_safe(feedback, feedback_tmp,
			&output->presentation_feedbacks, link) {
		presentation_feedback_destroy(feedback);
	}

	if (output->unmap_callback) {
		wl_callback_destroy(output->unmap_callback);
	}

	if (output->drm_syncobj_surface_v1) {
		wp_linux_drm_syncobj_surface_v1_destroy(output->drm_syncobj_surface_v1);
	}
	if (output->zxdg_toplevel_decoration_v1) {
		zxdg_toplevel_decoration_v1_destroy(output->zxdg_toplevel_decoration_v1);
	}
	if (output->xdg_toplevel) {
		xdg_toplevel_destroy(output->xdg_toplevel);
	}
	if (output->xdg_surface) {
		xdg_surface_destroy(output->xdg_surface);
	}
	if (output->own_surface) {
		wl_surface_destroy(output->surface);
	}
	wl_display_flush(output->backend->remote_display);

	free(output->title);
	free(output->app_id);

	free(output);
}

void update_wl_output_cursor(struct wlr_wl_output *output) {
	struct wlr_wl_pointer *pointer = output->cursor.pointer;
	if (pointer) {
		assert(pointer->output == output);
		assert(output->enter_serial);

		struct wlr_wl_seat *seat = pointer->seat;
		wl_pointer_set_cursor(seat->wl_pointer, output->enter_serial,
			output->cursor.surface, output->cursor.hotspot_x,
			output->cursor.hotspot_y);
	}
}

static bool output_move_cursor(struct wlr_output *_output, int x, int y) {
	// TODO: only return true if x == current x and y == current y
	return true;
}

static const struct wlr_output_impl output_impl = {
	.destroy = output_destroy,
	.test = output_test,
	.commit = output_commit,
	.set_cursor = output_set_cursor,
	.move_cursor = output_move_cursor,
	.get_cursor_formats = output_get_formats,
	.get_primary_formats = output_get_formats,
};

bool wlr_output_is_wl(struct wlr_output *wlr_output) {
	return wlr_output->impl == &output_impl;
}

static void xdg_surface_handle_configure(void *data,
		struct xdg_surface *xdg_surface, uint32_t serial) {
	struct wlr_wl_output *output = data;
	assert(output && output->xdg_surface == xdg_surface);

	int32_t req_width = output->wlr_output.width;
	int32_t req_height = output->wlr_output.height;
	if (output->requested_width > 0) {
		req_width = output->requested_width;
		output->requested_width = 0;
	}
	if (output->requested_height > 0) {
		req_height = output->requested_height;
		output->requested_height = 0;
	}

	if (output->unmap_callback != NULL) {
		return;
	}

	output->configured = true;
	output->has_configure_serial = true;
	output->configure_serial = serial;

	if (!output->wlr_output.enabled) {
		// We're waiting for a configure event after an initial commit to enable
		// the output. Do not notify the compositor about the requested state.
		return;
	}

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_custom_mode(&state, req_width, req_height, 0);
	wlr_output_send_request_state(&output->wlr_output, &state);
	wlr_output_state_finish(&state);
}

static const struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_handle_configure,
};

static void xdg_toplevel_handle_configure(void *data,
		struct xdg_toplevel *xdg_toplevel,
		int32_t width, int32_t height, struct wl_array *states) {
	struct wlr_wl_output *output = data;
	assert(output && output->xdg_toplevel == xdg_toplevel);

	if (width > 0) {
		output->requested_width = width;
	}
	if (height > 0) {
		output->requested_height = height;
	}
}

static void xdg_toplevel_handle_close(void *data,
		struct xdg_toplevel *xdg_toplevel) {
	struct wlr_wl_output *output = data;
	assert(output && output->xdg_toplevel == xdg_toplevel);

	wlr_output_destroy(&output->wlr_output);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
	.configure = xdg_toplevel_handle_configure,
	.close = xdg_toplevel_handle_close,
};

static struct wlr_wl_output *output_create(struct wlr_wl_backend *backend,
		struct wl_surface *surface) {
	struct wlr_wl_output *output = calloc(1, sizeof(*output));
	if (output == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate wlr_wl_output");
		return NULL;
	}
	struct wlr_output *wlr_output = &output->wlr_output;

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_custom_mode(&state, 1280, 720, 0);

	wlr_output_init(wlr_output, &backend->backend, &output_impl,
		backend->event_loop, &state);
	wlr_output_state_finish(&state);

	wlr_output->adaptive_sync_status = WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED;

	size_t output_num = ++last_output_num;

	char name[64];
	snprintf(name, sizeof(name), "WL-%zu", output_num);
	wlr_output_set_name(wlr_output, name);

	char description[128];
	snprintf(description, sizeof(description), "Wayland output %zu", output_num);
	wlr_output_set_description(wlr_output, description);

	output->surface = surface;
	output->backend = backend;
	wl_list_init(&output->presentation_feedbacks);

	wl_proxy_set_tag((struct wl_proxy *)output->surface, &surface_tag);
	wl_surface_set_user_data(output->surface, output);

	wl_list_insert(&backend->outputs, &output->link);

	return output;
}

static void output_start(struct wlr_wl_output *output) {
	struct wlr_output *wlr_output = &output->wlr_output;
	struct wlr_wl_backend *backend = output->backend;

	wl_signal_emit_mutable(&backend->backend.events.new_output, wlr_output);

	struct wlr_wl_seat *seat;
	wl_list_for_each(seat, &backend->seats, link) {
		if (seat->wl_pointer) {
			create_pointer(seat, output);
		}
	}
}

struct wlr_output *wlr_wl_output_create(struct wlr_backend *wlr_backend) {
	struct wlr_wl_backend *backend = get_wl_backend_from_backend(wlr_backend);
	if (!backend->started) {
		++backend->requested_outputs;
		return NULL;
	}

	struct wl_surface *surface = wl_compositor_create_surface(backend->compositor);
	if (surface == NULL) {
		wlr_log(WLR_ERROR, "Could not create output surface");
		return NULL;
	}

	struct wlr_wl_output *output = output_create(backend, surface);
	if (output == NULL) {
		wl_surface_destroy(surface);
		return NULL;
	}

	output->own_surface = true;

	output->xdg_surface =
		xdg_wm_base_get_xdg_surface(backend->xdg_wm_base, output->surface);
	if (!output->xdg_surface) {
		wlr_log_errno(WLR_ERROR, "Could not get xdg surface");
		goto error;
	}

	output->xdg_toplevel =
		xdg_surface_get_toplevel(output->xdg_surface);
	if (!output->xdg_toplevel) {
		wlr_log_errno(WLR_ERROR, "Could not get xdg toplevel");
		goto error;
	}

	if (backend->zxdg_decoration_manager_v1) {
		output->zxdg_toplevel_decoration_v1 =
			zxdg_decoration_manager_v1_get_toplevel_decoration(
			backend->zxdg_decoration_manager_v1, output->xdg_toplevel);
		if (!output->zxdg_toplevel_decoration_v1) {
			wlr_log_errno(WLR_ERROR, "Could not get xdg toplevel decoration");
			goto error;
		}
		zxdg_toplevel_decoration_v1_set_mode(output->zxdg_toplevel_decoration_v1,
			ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
	}

	if (!update_title(output, NULL)) {
		wlr_log_errno(WLR_ERROR, "Could not allocate xdg toplevel title");
		goto error;
	}
	if (!update_app_id(output, NULL)) {
		wlr_log_errno(WLR_ERROR, "Could not allocate xdg toplevel app_id");
		goto error;
	}

	xdg_surface_add_listener(output->xdg_surface,
			&xdg_surface_listener, output);
	xdg_toplevel_add_listener(output->xdg_toplevel,
			&xdg_toplevel_listener, output);

	wl_display_flush(backend->remote_display);

	output_start(output);

	// TODO: let the compositor do this bit
	if (backend->activation_v1 && backend->activation_token) {
		xdg_activation_v1_activate(backend->activation_v1,
				backend->activation_token, output->surface);
	}

	return &output->wlr_output;

error:
	wlr_output_destroy(&output->wlr_output);
	return NULL;
}

struct wlr_output *wlr_wl_output_create_from_surface(struct wlr_backend *wlr_backend,
		struct wl_surface *surface) {
	struct wlr_wl_backend *backend = get_wl_backend_from_backend(wlr_backend);
	assert(backend->started);

	struct wlr_wl_output *output = output_create(backend, surface);
	if (output == NULL) {
		wl_surface_destroy(surface);
		return NULL;
	}

	output_start(output);

	return &output->wlr_output;
}

void wlr_wl_output_set_title(struct wlr_output *output, const char *title) {
	struct wlr_wl_output *wl_output = get_wl_output_from_output(output);
	assert(wl_output->xdg_toplevel != NULL);

	if (update_title(wl_output, title) && wl_output->initialized) {
		xdg_toplevel_set_title(wl_output->xdg_toplevel, wl_output->title);
		wl_display_flush(wl_output->backend->remote_display);
	}
}

void wlr_wl_output_set_app_id(struct wlr_output *output, const char *app_id) {
	struct wlr_wl_output *wl_output = get_wl_output_from_output(output);
	assert(wl_output->xdg_toplevel != NULL);

	if (update_app_id(wl_output, app_id) && wl_output->initialized) {
		xdg_toplevel_set_app_id(wl_output->xdg_toplevel, wl_output->app_id);
		wl_display_flush(wl_output->backend->remote_display);
	}
}

struct wl_surface *wlr_wl_output_get_surface(struct wlr_output *output) {
	struct wlr_wl_output *wl_output = get_wl_output_from_output(output);
	return wl_output->surface;
}
