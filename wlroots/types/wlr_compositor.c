#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wlr/render/interface.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wlr/util/transform.h>
#include "types/wlr_buffer.h"
#include "types/wlr_region.h"
#include "types/wlr_subcompositor.h"
#include "util/array.h"
#include "util/time.h"

#define COMPOSITOR_VERSION 6
#define CALLBACK_VERSION 1

static int min(int fst, int snd) {
	if (fst < snd) {
		return fst;
	} else {
		return snd;
	}
}

static int max(int fst, int snd) {
	if (fst > snd) {
		return fst;
	} else {
		return snd;
	}
}

static void set_pending_buffer_resource(struct wlr_surface *surface,
		struct wl_resource *resource) {
	wl_list_remove(&surface->pending_buffer_resource_destroy.link);
	surface->pending_buffer_resource = resource;
	if (resource != NULL) {
		wl_resource_add_destroy_listener(resource, &surface->pending_buffer_resource_destroy);
	} else {
		wl_list_init(&surface->pending_buffer_resource_destroy.link);
	}
}

static void pending_buffer_resource_handle_destroy(struct wl_listener *listener, void *data) {
	struct wlr_surface *surface =
		wl_container_of(listener, surface, pending_buffer_resource_destroy);

	set_pending_buffer_resource(surface, NULL);
}

static void surface_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);
	if (surface->role_resource != NULL) {
		wl_resource_post_error(resource,
			WL_SURFACE_ERROR_DEFUNCT_ROLE_OBJECT,
			"surface was destroyed before its role object");
		return;
	}
	wl_resource_destroy(resource);
}

static void surface_handle_attach(struct wl_client *client,
		struct wl_resource *resource,
		struct wl_resource *buffer_resource, int32_t dx, int32_t dy) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);

	if (wl_resource_get_version(resource) >= WL_SURFACE_OFFSET_SINCE_VERSION &&
			(dx != 0 || dy != 0)) {
		wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_OFFSET,
			"Offset must be zero on wl_surface.attach version >= %"PRIu32,
			WL_SURFACE_OFFSET_SINCE_VERSION);
		return;
	}

	surface->pending.committed |= WLR_SURFACE_STATE_BUFFER;
	set_pending_buffer_resource(surface, buffer_resource);

	if (wl_resource_get_version(resource) < WL_SURFACE_OFFSET_SINCE_VERSION) {
		surface->pending.committed |= WLR_SURFACE_STATE_OFFSET;
		surface->pending.dx = dx;
		surface->pending.dy = dy;
	}
}

static void surface_handle_damage(struct wl_client *client,
		struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);
	if (width < 0 || height < 0) {
		return;
	}
	surface->pending.committed |= WLR_SURFACE_STATE_SURFACE_DAMAGE;
	pixman_region32_union_rect(&surface->pending.surface_damage,
		&surface->pending.surface_damage,
		x, y, width, height);
}

static void callback_handle_resource_destroy(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static void surface_handle_frame(struct wl_client *client,
		struct wl_resource *resource, uint32_t callback) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);

	struct wl_resource *callback_resource = wl_resource_create(client,
		&wl_callback_interface, CALLBACK_VERSION, callback);
	if (callback_resource == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}
	wl_resource_set_implementation(callback_resource, NULL, NULL,
		callback_handle_resource_destroy);

	wl_list_insert(surface->pending.frame_callback_list.prev,
		wl_resource_get_link(callback_resource));

	surface->pending.committed |= WLR_SURFACE_STATE_FRAME_CALLBACK_LIST;
}

static void surface_handle_set_opaque_region(struct wl_client *client,
		struct wl_resource *resource,
		struct wl_resource *region_resource) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);
	surface->pending.committed |= WLR_SURFACE_STATE_OPAQUE_REGION;
	if (region_resource) {
		const pixman_region32_t *region = wlr_region_from_resource(region_resource);
		pixman_region32_copy(&surface->pending.opaque, region);
	} else {
		pixman_region32_clear(&surface->pending.opaque);
	}
}

static void surface_handle_set_input_region(struct wl_client *client,
		struct wl_resource *resource,
		struct wl_resource *region_resource) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);
	surface->pending.committed |= WLR_SURFACE_STATE_INPUT_REGION;
	if (region_resource) {
		const pixman_region32_t *region = wlr_region_from_resource(region_resource);
		pixman_region32_copy(&surface->pending.input, region);
	} else {
		pixman_region32_fini(&surface->pending.input);
		pixman_region32_init_rect(&surface->pending.input,
			INT32_MIN, INT32_MIN, UINT32_MAX, UINT32_MAX);
	}
}

static void surface_state_transformed_buffer_size(struct wlr_surface_state *state,
		int *width, int *height) {
	*width = state->buffer_width;
	*height = state->buffer_height;
	wlr_output_transform_coords(state->transform, width, height);
}

/**
 * Computes the surface viewport source size, ie. the size after applying the
 * surface's scale, transform and cropping (via the viewport's source
 * rectangle) but before applying the viewport scaling (via the viewport's
 * destination rectangle).
 */
static void surface_state_viewport_src_size(struct wlr_surface_state *state,
		int *out_width, int *out_height) {
	if (state->buffer_width == 0 && state->buffer_height == 0) {
		*out_width = *out_height = 0;
		return;
	}

	if (state->viewport.has_src) {
		*out_width = state->viewport.src.width;
		*out_height = state->viewport.src.height;
	} else {
		surface_state_transformed_buffer_size(state,
			out_width, out_height);
		*out_width /= state->scale;
		*out_height /= state->scale;
	}
}

static void surface_finalize_pending(struct wlr_surface *surface) {
	struct wlr_surface_state *pending = &surface->pending;

	if ((pending->committed & WLR_SURFACE_STATE_BUFFER)) {
		struct wl_resource *buffer_resource = surface->pending_buffer_resource;
		if (buffer_resource != NULL) {
			set_pending_buffer_resource(surface, NULL);

			pending->buffer = wlr_buffer_try_from_resource(buffer_resource);
			if (pending->buffer == NULL) {
				wlr_surface_reject_pending(surface,
					buffer_resource, -1, "unknown buffer type");
			}
		}

		if (pending->buffer != NULL) {
			pending->buffer_width = pending->buffer->width;
			pending->buffer_height = pending->buffer->height;
		} else {
			pending->buffer_width = pending->buffer_height = 0;
		}
	}

	if (!pending->viewport.has_src &&
			(pending->buffer_width % pending->scale != 0 ||
			pending->buffer_height % pending->scale != 0)) {
		// TODO: send WL_SURFACE_ERROR_INVALID_SIZE error to cursor surfaces
		// once this issue is resolved:
		// https://gitlab.freedesktop.org/wayland/wayland/-/issues/194
		if (!surface->role
				|| strcmp(surface->role->name, "wl_pointer-cursor") == 0
				|| strcmp(surface->role->name, "wp_tablet_tool-cursor") == 0) {
			wlr_log(WLR_DEBUG, "Client bug: submitted a buffer whose size (%dx%d) "
				"is not divisible by scale (%d)", pending->buffer_width,
				pending->buffer_height, pending->scale);
		} else {
			wlr_surface_reject_pending(surface, surface->resource,
				WL_SURFACE_ERROR_INVALID_SIZE,
				"Buffer size (%dx%d) is not divisible by scale (%d)",
				pending->buffer_width, pending->buffer_height, pending->scale);
		}
	}

	if (pending->viewport.has_dst) {
		if (pending->buffer_width == 0 && pending->buffer_height == 0) {
			pending->width = pending->height = 0;
		} else {
			pending->width = pending->viewport.dst_width;
			pending->height = pending->viewport.dst_height;
		}
	} else {
		surface_state_viewport_src_size(pending, &pending->width, &pending->height);
	}

	pixman_region32_intersect_rect(&pending->surface_damage,
		&pending->surface_damage, 0, 0, pending->width, pending->height);

	pixman_region32_intersect_rect(&pending->buffer_damage,
		&pending->buffer_damage, 0, 0, pending->buffer_width,
		pending->buffer_height);
}

static void surface_update_damage(pixman_region32_t *buffer_damage,
		struct wlr_surface_state *current, struct wlr_surface_state *pending) {
	pixman_region32_clear(buffer_damage);

	// Copy over surface damage + buffer damage
	pixman_region32_t surface_damage;
	pixman_region32_init(&surface_damage);

	pixman_region32_copy(&surface_damage, &pending->surface_damage);

	if (pending->viewport.has_dst) {
		int src_width, src_height;
		surface_state_viewport_src_size(pending, &src_width, &src_height);
		float scale_x = (float)pending->viewport.dst_width / src_width;
		float scale_y = (float)pending->viewport.dst_height / src_height;
		wlr_region_scale_xy(&surface_damage, &surface_damage,
			1.0 / scale_x, 1.0 / scale_y);
	}
	if (pending->viewport.has_src) {
		// This is lossy: do a best-effort conversion
		pixman_region32_translate(&surface_damage,
			floor(pending->viewport.src.x),
			floor(pending->viewport.src.y));
	}

	wlr_region_scale(&surface_damage, &surface_damage, pending->scale);

	int width, height;
	surface_state_transformed_buffer_size(pending, &width, &height);
	wlr_region_transform(&surface_damage, &surface_damage,
		wlr_output_transform_invert(pending->transform),
		width, height);

	pixman_region32_union(buffer_damage,
		&pending->buffer_damage, &surface_damage);

	pixman_region32_fini(&surface_damage);
}

static void *surface_synced_create_state(struct wlr_surface_synced *synced) {
	void *state = calloc(1, synced->impl->state_size);
	if (state == NULL) {
		return NULL;
	}
	if (synced->impl->init_state) {
		synced->impl->init_state(state);
	}
	return state;
}

static void surface_synced_destroy_state(struct wlr_surface_synced *synced,
		void *state) {
	if (state == NULL) {
		return;
	}
	if (synced->impl->finish_state) {
		synced->impl->finish_state(state);
	}
	free(state);
}

static void surface_synced_move_state(struct wlr_surface_synced *synced,
		void *dst, void *src) {
	if (synced->impl->move_state) {
		synced->impl->move_state(dst, src);
	} else {
		memcpy(dst, src, synced->impl->state_size);
	}
}

/**
 * Overwrite state with a copy of the next state, then clear the next state.
 */
static void surface_state_move(struct wlr_surface_state *state,
		struct wlr_surface_state *next, struct wlr_surface *surface) {
	state->width = next->width;
	state->height = next->height;
	state->buffer_width = next->buffer_width;
	state->buffer_height = next->buffer_height;

	if (next->committed & WLR_SURFACE_STATE_SCALE) {
		state->scale = next->scale;
	}
	if (next->committed & WLR_SURFACE_STATE_TRANSFORM) {
		state->transform = next->transform;
	}
	if (next->committed & WLR_SURFACE_STATE_OFFSET) {
		state->dx = next->dx;
		state->dy = next->dy;
		next->dx = next->dy = 0;
	} else {
		state->dx = state->dy = 0;
	}
	if (next->committed & WLR_SURFACE_STATE_BUFFER) {
		wlr_buffer_unlock(state->buffer);
		state->buffer = NULL;
		if (next->buffer) {
			state->buffer = wlr_buffer_lock(next->buffer);
		}
		wlr_buffer_unlock(next->buffer);
		next->buffer = NULL;
	}
	if (next->committed & WLR_SURFACE_STATE_SURFACE_DAMAGE) {
		pixman_region32_copy(&state->surface_damage, &next->surface_damage);
		pixman_region32_clear(&next->surface_damage);
	} else {
		pixman_region32_clear(&state->surface_damage);
	}
	if (next->committed & WLR_SURFACE_STATE_BUFFER_DAMAGE) {
		pixman_region32_copy(&state->buffer_damage, &next->buffer_damage);
		pixman_region32_clear(&next->buffer_damage);
	} else {
		pixman_region32_clear(&state->buffer_damage);
	}
	if (next->committed & WLR_SURFACE_STATE_OPAQUE_REGION) {
		pixman_region32_copy(&state->opaque, &next->opaque);
	}
	if (next->committed & WLR_SURFACE_STATE_INPUT_REGION) {
		pixman_region32_copy(&state->input, &next->input);
	}
	if (next->committed & WLR_SURFACE_STATE_VIEWPORT) {
		state->viewport = next->viewport;
	}
	if (next->committed & WLR_SURFACE_STATE_FRAME_CALLBACK_LIST) {
		wl_list_insert_list(&state->frame_callback_list,
			&next->frame_callback_list);
		wl_list_init(&next->frame_callback_list);
	}

	void **state_synced = state->synced.data;
	void **next_synced = next->synced.data;
	struct wlr_surface_synced *synced;
	wl_list_for_each(synced, &surface->synced, link) {
		surface_synced_move_state(synced,
			state_synced[synced->index], next_synced[synced->index]);
	}

	// commit subsurface order
	struct wlr_subsurface_parent_state *sub_state_next, *sub_state;
	wl_list_for_each(sub_state_next, &next->subsurfaces_below, link) {
		sub_state = wlr_surface_synced_get_state(sub_state_next->synced, state);
		wl_list_remove(&sub_state->link);
		wl_list_insert(state->subsurfaces_below.prev, &sub_state->link);
	}
	wl_list_for_each(sub_state_next, &next->subsurfaces_above, link) {
		sub_state = wlr_surface_synced_get_state(sub_state_next->synced, state);
		wl_list_remove(&sub_state->link);
		wl_list_insert(state->subsurfaces_above.prev, &sub_state->link);
	}

	state->committed = next->committed;
	next->committed = 0;

	state->seq = next->seq;

	state->cached_state_locks = next->cached_state_locks;
	next->cached_state_locks = 0;
}

static void surface_apply_damage(struct wlr_surface *surface) {
	if (surface->current.buffer == NULL) {
		// NULL commit
		if (surface->buffer != NULL) {
			wlr_buffer_unlock(&surface->buffer->base);
		}
		surface->buffer = NULL;
		surface->opaque = false;
		return;
	}

	surface->opaque = wlr_buffer_is_opaque(surface->current.buffer);

	if (surface->buffer != NULL) {
		if (wlr_client_buffer_apply_damage(surface->buffer,
				surface->current.buffer, &surface->buffer_damage)) {
			wlr_buffer_unlock(surface->current.buffer);
			surface->current.buffer = NULL;
			return;
		}
	}

	if (surface->compositor->renderer == NULL) {
		return;
	}

	struct wlr_client_buffer *buffer = wlr_client_buffer_create(
			surface->current.buffer, surface->compositor->renderer);

	if (buffer == NULL) {
		wlr_log(WLR_ERROR, "Failed to upload buffer");
		return;
	}

	if (surface->buffer != NULL) {
		wlr_buffer_unlock(&surface->buffer->base);
	}
	surface->buffer = buffer;
}

static void surface_update_opaque_region(struct wlr_surface *surface) {
	if (!wlr_surface_has_buffer(surface)) {
		pixman_region32_clear(&surface->opaque_region);
		return;
	}

	if (surface->opaque) {
		pixman_region32_fini(&surface->opaque_region);
		pixman_region32_init_rect(&surface->opaque_region,
			0, 0, surface->current.width, surface->current.height);
		return;
	}

	pixman_region32_intersect_rect(&surface->opaque_region,
		&surface->current.opaque,
		0, 0, surface->current.width, surface->current.height);
}

static void surface_update_input_region(struct wlr_surface *surface) {
	pixman_region32_intersect_rect(&surface->input_region,
		&surface->current.input,
		0, 0, surface->current.width, surface->current.height);
}

static bool surface_state_init(struct wlr_surface_state *state,
	struct wlr_surface *surface);
static void surface_state_finish(struct wlr_surface_state *state);

static void surface_cache_pending(struct wlr_surface *surface) {
	struct wlr_surface_state *cached = calloc(1, sizeof(*cached));
	if (!cached) {
		goto error;
	}

	if (!surface_state_init(cached, surface)) {
		goto error_cached;
	}

	void **cached_synced = cached->synced.data;
	struct wlr_surface_synced *synced;
	wl_list_for_each(synced, &surface->synced, link) {
		void *synced_state = surface_synced_create_state(synced);
		if (synced_state == NULL) {
			goto error_state;
		}
		cached_synced[synced->index] = synced_state;
	}

	surface_state_move(cached, &surface->pending, surface);

	wl_list_insert(surface->cached.prev, &cached->cached_state_link);

	surface->pending.seq++;

	return;

error_state:
	surface_state_finish(cached);
error_cached:
	free(cached);
error:
	wl_resource_post_no_memory(surface->resource);
}

static void surface_commit_state(struct wlr_surface *surface,
		struct wlr_surface_state *next) {
	assert(next->cached_state_locks == 0);

	bool invalid_buffer = next->committed & WLR_SURFACE_STATE_BUFFER;

	if (invalid_buffer && next->buffer == NULL) {
		surface->unmap_commit = surface->mapped;
		wlr_surface_unmap(surface);
	} else {
		surface->unmap_commit = false;
	}

	surface_update_damage(&surface->buffer_damage, &surface->current, next);

	surface->previous.scale = surface->current.scale;
	surface->previous.transform = surface->current.transform;
	surface->previous.width = surface->current.width;
	surface->previous.height = surface->current.height;
	surface->previous.buffer_width = surface->current.buffer_width;
	surface->previous.buffer_height = surface->current.buffer_height;

	surface_state_move(&surface->current, next, surface);

	if (invalid_buffer) {
		surface_apply_damage(surface);
	}
	surface_update_opaque_region(surface);
	surface_update_input_region(surface);

	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
		subsurface_handle_parent_commit(subsurface);
	}
	wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
		subsurface_handle_parent_commit(subsurface);
	}

	// If we're committing the pending state, bump the pending sequence number
	// here, to allow commit listeners to lock the new pending state.
	if (next == &surface->pending) {
		surface->pending.seq++;
	}

	struct wlr_surface_synced *synced;
	wl_list_for_each(synced, &surface->synced, link) {
		if (synced->impl->commit) {
			synced->impl->commit(synced);
		}
	}

	if (surface->role != NULL && surface->role->commit != NULL &&
			(surface->role_resource != NULL || surface->role->no_object)) {
		surface->role->commit(surface);
	}

	wl_signal_emit_mutable(&surface->events.commit, surface);

	// Release the buffer after emitting the commit event, so that listeners can
	// access it. Don't leave the buffer locked so that wl_shm buffers can be
	// released immediately on commit when they are uploaded to the GPU.
	wlr_buffer_unlock(surface->current.buffer);
	surface->current.buffer = NULL;
}

static void surface_handle_commit(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);
	surface->handling_commit = true;

	surface_finalize_pending(surface);

	if (surface->role != NULL && surface->role->client_commit != NULL &&
			(surface->role_resource != NULL || surface->role->no_object)) {
		surface->role->client_commit(surface);
	}

	wl_signal_emit_mutable(&surface->events.client_commit, NULL);

	surface->handling_commit = false;
	if (surface->pending_rejected) {
		return;
	}

	if (surface->pending.cached_state_locks > 0 || !wl_list_empty(&surface->cached)) {
		surface_cache_pending(surface);
	} else {
		surface_commit_state(surface, &surface->pending);
	}
}

static void surface_handle_set_buffer_transform(struct wl_client *client,
		struct wl_resource *resource, int32_t transform) {
	if (transform < WL_OUTPUT_TRANSFORM_NORMAL ||
			transform > WL_OUTPUT_TRANSFORM_FLIPPED_270) {
		wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_TRANSFORM,
			"Specified transform value (%d) is invalid", transform);
		return;
	}
	struct wlr_surface *surface = wlr_surface_from_resource(resource);
	surface->pending.committed |= WLR_SURFACE_STATE_TRANSFORM;
	surface->pending.transform = transform;
}

static void surface_handle_set_buffer_scale(struct wl_client *client,
		struct wl_resource *resource, int32_t scale) {
	if (scale <= 0) {
		wl_resource_post_error(resource, WL_SURFACE_ERROR_INVALID_SCALE,
			"Specified scale value (%d) is not positive", scale);
		return;
	}
	struct wlr_surface *surface = wlr_surface_from_resource(resource);
	surface->pending.committed |= WLR_SURFACE_STATE_SCALE;
	surface->pending.scale = scale;
}

static void surface_handle_damage_buffer(struct wl_client *client,
		struct wl_resource *resource,
		int32_t x, int32_t y, int32_t width,
		int32_t height) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);
	if (width < 0 || height < 0) {
		return;
	}
	surface->pending.committed |= WLR_SURFACE_STATE_BUFFER_DAMAGE;
	pixman_region32_union_rect(&surface->pending.buffer_damage,
		&surface->pending.buffer_damage,
		x, y, width, height);
}

static void surface_handle_offset(struct wl_client *client,
		struct wl_resource *resource, int32_t x, int32_t y) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);

	surface->pending.committed |= WLR_SURFACE_STATE_OFFSET;
	surface->pending.dx = x;
	surface->pending.dy = y;
}

static const struct wl_surface_interface surface_implementation = {
	.destroy = surface_handle_destroy,
	.attach = surface_handle_attach,
	.damage = surface_handle_damage,
	.frame = surface_handle_frame,
	.set_opaque_region = surface_handle_set_opaque_region,
	.set_input_region = surface_handle_set_input_region,
	.commit = surface_handle_commit,
	.set_buffer_transform = surface_handle_set_buffer_transform,
	.set_buffer_scale = surface_handle_set_buffer_scale,
	.damage_buffer = surface_handle_damage_buffer,
	.offset = surface_handle_offset,
};

struct wlr_surface *wlr_surface_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wl_surface_interface,
		&surface_implementation));
	return wl_resource_get_user_data(resource);
}

static bool surface_state_init(struct wlr_surface_state *state,
		struct wlr_surface *surface) {
	*state = (struct wlr_surface_state){
		.scale = 1,
		.transform = WL_OUTPUT_TRANSFORM_NORMAL,
	};

	wl_list_init(&state->subsurfaces_above);
	wl_list_init(&state->subsurfaces_below);

	wl_list_init(&state->frame_callback_list);

	pixman_region32_init(&state->surface_damage);
	pixman_region32_init(&state->buffer_damage);
	pixman_region32_init(&state->opaque);
	pixman_region32_init_rect(&state->input,
		INT32_MIN, INT32_MIN, UINT32_MAX, UINT32_MAX);

	wl_array_init(&state->synced);
	void *ptr = wl_array_add(&state->synced, surface->synced_len * sizeof(void *));
	return ptr != NULL;
}

static void surface_state_finish(struct wlr_surface_state *state) {
	wlr_buffer_unlock(state->buffer);

	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &state->frame_callback_list) {
		wl_resource_destroy(resource);
	}

	pixman_region32_fini(&state->surface_damage);
	pixman_region32_fini(&state->buffer_damage);
	pixman_region32_fini(&state->opaque);
	pixman_region32_fini(&state->input);

	wl_array_release(&state->synced);
}

static void surface_state_destroy_cached(struct wlr_surface_state *state,
		struct wlr_surface *surface) {
	void **synced_states = state->synced.data;
	struct wlr_surface_synced *synced;
	wl_list_for_each(synced, &surface->synced, link) {
		surface_synced_destroy_state(synced, synced_states[synced->index]);
	}

	surface_state_finish(state);
	wl_list_remove(&state->cached_state_link);
	free(state);
}

static void surface_output_destroy(struct wlr_surface_output *surface_output);
static void surface_destroy_role_object(struct wlr_surface *surface);

static void surface_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_surface *surface = wlr_surface_from_resource(resource);

	surface_destroy_role_object(surface);

	wl_signal_emit_mutable(&surface->events.destroy, surface);
	wlr_addon_set_finish(&surface->addons);

	assert(wl_list_empty(&surface->events.client_commit.listener_list));
	assert(wl_list_empty(&surface->events.commit.listener_list));
	assert(wl_list_empty(&surface->events.map.listener_list));
	assert(wl_list_empty(&surface->events.unmap.listener_list));
	assert(wl_list_empty(&surface->events.destroy.listener_list));
	assert(wl_list_empty(&surface->events.new_subsurface.listener_list));

	assert(wl_list_empty(&surface->synced));

	struct wlr_surface_state *cached, *cached_tmp;
	wl_list_for_each_safe(cached, cached_tmp, &surface->cached, cached_state_link) {
		surface_state_destroy_cached(cached, surface);
	}

	wl_list_remove(&surface->role_resource_destroy.link);

	wl_list_remove(&surface->pending_buffer_resource_destroy.link);

	surface_state_finish(&surface->pending);
	surface_state_finish(&surface->current);
	pixman_region32_fini(&surface->buffer_damage);
	pixman_region32_fini(&surface->opaque_region);
	pixman_region32_fini(&surface->input_region);
	if (surface->buffer != NULL) {
		wlr_buffer_unlock(&surface->buffer->base);
	}

	struct wlr_surface_output *surface_output, *surface_output_tmp;
	wl_list_for_each_safe(surface_output, surface_output_tmp,
			&surface->current_outputs, link) {
		surface_output_destroy(surface_output);
	}

	free(surface);
}

static struct wlr_surface *surface_create(struct wl_client *client,
		uint32_t version, uint32_t id, struct wlr_compositor *compositor) {
	struct wlr_surface *surface = calloc(1, sizeof(*surface));
	if (!surface) {
		wl_client_post_no_memory(client);
		return NULL;
	}
	surface->resource = wl_resource_create(client, &wl_surface_interface,
		version, id);
	if (surface->resource == NULL) {
		free(surface);
		wl_client_post_no_memory(client);
		return NULL;
	}
	wl_resource_set_implementation(surface->resource, &surface_implementation,
		surface, surface_handle_resource_destroy);

	wlr_log(WLR_DEBUG, "New wlr_surface %p (res %p)", surface, surface->resource);

	surface->compositor = compositor;

	surface_state_init(&surface->current, surface);
	surface_state_init(&surface->pending, surface);
	surface->pending.seq = 1;

	wl_signal_init(&surface->events.client_commit);
	wl_signal_init(&surface->events.commit);
	wl_signal_init(&surface->events.map);
	wl_signal_init(&surface->events.unmap);
	wl_signal_init(&surface->events.destroy);
	wl_signal_init(&surface->events.new_subsurface);

	wl_list_init(&surface->current_outputs);
	wl_list_init(&surface->cached);
	pixman_region32_init(&surface->buffer_damage);
	pixman_region32_init(&surface->opaque_region);
	pixman_region32_init(&surface->input_region);
	wlr_addon_set_init(&surface->addons);
	wl_list_init(&surface->synced);

	wl_list_init(&surface->role_resource_destroy.link);

	surface->pending_buffer_resource_destroy.notify = pending_buffer_resource_handle_destroy;
	wl_list_init(&surface->pending_buffer_resource_destroy.link);

	return surface;
}

struct wlr_texture *wlr_surface_get_texture(struct wlr_surface *surface) {
	if (surface->buffer == NULL) {
		return NULL;
	}
	return surface->buffer->texture;
}

bool wlr_surface_has_buffer(struct wlr_surface *surface) {
	return wlr_surface_state_has_buffer(&surface->current);
}

bool wlr_surface_state_has_buffer(const struct wlr_surface_state *state) {
	return state->buffer_width > 0 && state->buffer_height > 0;
}

void wlr_surface_map(struct wlr_surface *surface) {
	if (surface->mapped) {
		return;
	}
	assert(wlr_surface_has_buffer(surface));
	surface->mapped = true;

	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
		subsurface_consider_map(subsurface);
	}
	wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
		subsurface_consider_map(subsurface);
	}

	if (surface->role != NULL && surface->role->map != NULL &&
			(surface->role_resource != NULL || surface->role->no_object)) {
		surface->role->map(surface);
	}

	wl_signal_emit_mutable(&surface->events.map, NULL);
}

void wlr_surface_unmap(struct wlr_surface *surface) {
	if (!surface->mapped) {
		return;
	}
	surface->mapped = false;
	wl_signal_emit_mutable(&surface->events.unmap, NULL);
	if (surface->role != NULL && surface->role->unmap != NULL &&
			(surface->role_resource != NULL || surface->role->no_object)) {
		surface->role->unmap(surface);
	}

	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
		wlr_surface_unmap(subsurface->surface);
	}
	wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
		wlr_surface_unmap(subsurface->surface);
	}
}

void wlr_surface_reject_pending(struct wlr_surface *surface, struct wl_resource *resource,
		uint32_t code, const char *msg, ...) {
	assert(surface->handling_commit);
	if (surface->pending_rejected) {
		return;
	}

	va_list args;
	va_start(args, msg);

	wl_resource_post_error_vargs(resource, code, msg, args);
	surface->pending_rejected = true;

	va_end(args);
}

bool wlr_surface_set_role(struct wlr_surface *surface, const struct wlr_surface_role *role,
		struct wl_resource *error_resource, uint32_t error_code) {
	assert(role != NULL);

	if (surface->role != NULL && surface->role != role) {
		if (error_resource != NULL) {
			wl_resource_post_error(error_resource, error_code,
				"Cannot assign role %s to wl_surface@%" PRIu32 ", already has role %s",
				role->name, wl_resource_get_id(surface->resource),
				surface->role->name);
		}
		return false;
	}
	if (surface->role_resource != NULL) {
		wl_resource_post_error(error_resource, error_code,
			"Cannot reassign role %s to wl_surface@%" PRIu32 ", role object still exists",
			role->name, wl_resource_get_id(surface->resource));
		return false;
	}

	surface->role = role;
	return true;
}

static void surface_handle_role_resource_destroy(struct wl_listener *listener, void *data) {
	struct wlr_surface *surface = wl_container_of(listener, surface, role_resource_destroy);
	surface_destroy_role_object(surface);
}

void wlr_surface_set_role_object(struct wlr_surface *surface, struct wl_resource *role_resource) {
	assert(surface->role != NULL);
	assert(!surface->role->no_object);
	assert(surface->role_resource == NULL);
	assert(role_resource != NULL);
	surface->role_resource = role_resource;
	surface->role_resource_destroy.notify = surface_handle_role_resource_destroy;
	wl_resource_add_destroy_listener(role_resource, &surface->role_resource_destroy);
}

static void surface_destroy_role_object(struct wlr_surface *surface) {
	if (surface->role_resource == NULL) {
		return;
	}
	wlr_surface_unmap(surface);
	if (surface->role->destroy != NULL) {
		surface->role->destroy(surface);
	}
	surface->role_resource = NULL;
	wl_list_remove(&surface->role_resource_destroy.link);
	wl_list_init(&surface->role_resource_destroy.link);
}

uint32_t wlr_surface_lock_pending(struct wlr_surface *surface) {
	surface->pending.cached_state_locks++;
	return surface->pending.seq;
}

void wlr_surface_unlock_cached(struct wlr_surface *surface, uint32_t seq) {
	if (surface->pending.seq == seq) {
		assert(surface->pending.cached_state_locks > 0);
		surface->pending.cached_state_locks--;
		return;
	}

	bool found = false;
	struct wlr_surface_state *cached;
	wl_list_for_each(cached, &surface->cached, cached_state_link) {
		if (cached->seq == seq) {
			found = true;
			break;
		}
	}
	assert(found);

	assert(cached->cached_state_locks > 0);
	cached->cached_state_locks--;

	if (cached->cached_state_locks != 0) {
		return;
	}

	if (cached->cached_state_link.prev != &surface->cached) {
		// This isn't the first cached state. This means we're blocked on a
		// previous cached state.
		return;
	}

	// TODO: consider merging all committed states together
	struct wlr_surface_state *next, *tmp;
	wl_list_for_each_safe(next, tmp, &surface->cached, cached_state_link) {
		if (next->cached_state_locks > 0) {
			break;
		}

		surface_commit_state(surface, next);
		surface_state_destroy_cached(next, surface);
	}
}

struct wlr_surface *wlr_surface_get_root_surface(struct wlr_surface *surface) {
	struct wlr_subsurface *subsurface;
	while ((subsurface = wlr_subsurface_try_from_wlr_surface(surface))) {
		surface = subsurface->parent;
	}
	return surface;
}

bool wlr_surface_point_accepts_input(struct wlr_surface *surface,
		double sx, double sy) {
	return sx >= 0 && sx < surface->current.width &&
		sy >= 0 && sy < surface->current.height &&
		pixman_region32_contains_point(&surface->input_region,
			floor(sx), floor(sy), NULL);
}

struct wlr_surface *wlr_surface_surface_at(struct wlr_surface *surface,
		double sx, double sy, double *sub_x, double *sub_y) {
	struct wlr_subsurface *subsurface;
	wl_list_for_each_reverse(subsurface, &surface->current.subsurfaces_above,
			current.link) {
		if (!subsurface->surface->mapped) {
			continue;
		}

		double _sub_x = subsurface->current.x;
		double _sub_y = subsurface->current.y;
		struct wlr_surface *sub = wlr_surface_surface_at(subsurface->surface,
			sx - _sub_x, sy - _sub_y, sub_x, sub_y);
		if (sub != NULL) {
			return sub;
		}
	}

	if (wlr_surface_point_accepts_input(surface, sx, sy)) {
		if (sub_x) {
			*sub_x = sx;
		}
		if (sub_y) {
			*sub_y = sy;
		}
		return surface;
	}

	wl_list_for_each_reverse(subsurface, &surface->current.subsurfaces_below,
			current.link) {
		if (!subsurface->surface->mapped) {
			continue;
		}

		double _sub_x = subsurface->current.x;
		double _sub_y = subsurface->current.y;
		struct wlr_surface *sub = wlr_surface_surface_at(subsurface->surface,
			sx - _sub_x, sy - _sub_y, sub_x, sub_y);
		if (sub != NULL) {
			return sub;
		}
	}

	return NULL;
}

static void surface_output_destroy(struct wlr_surface_output *surface_output) {
	wl_list_remove(&surface_output->bind.link);
	wl_list_remove(&surface_output->destroy.link);
	wl_list_remove(&surface_output->link);

	free(surface_output);
}

static void surface_handle_output_bind(struct wl_listener *listener,
		void *data) {
	struct wlr_output_event_bind *evt = data;
	struct wlr_surface_output *surface_output =
		wl_container_of(listener, surface_output, bind);
	struct wl_client *client = wl_resource_get_client(
			surface_output->surface->resource);
	if (client == wl_resource_get_client(evt->resource)) {
		wl_surface_send_enter(surface_output->surface->resource, evt->resource);
	}
}

static void surface_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_surface_output *surface_output =
		wl_container_of(listener, surface_output, destroy);
	surface_output_destroy(surface_output);
}

void wlr_surface_send_enter(struct wlr_surface *surface,
		struct wlr_output *output) {
	struct wl_client *client = wl_resource_get_client(surface->resource);
	struct wlr_surface_output *surface_output;
	struct wl_resource *resource;

	wl_list_for_each(surface_output, &surface->current_outputs, link) {
		if (surface_output->output == output) {
			return;
		}
	}

	surface_output = calloc(1, sizeof(*surface_output));
	if (surface_output == NULL) {
		return;
	}
	surface_output->bind.notify = surface_handle_output_bind;
	surface_output->destroy.notify = surface_handle_output_destroy;

	wl_signal_add(&output->events.bind, &surface_output->bind);
	wl_signal_add(&output->events.destroy, &surface_output->destroy);

	surface_output->surface = surface;
	surface_output->output = output;
	wl_list_insert(&surface->current_outputs, &surface_output->link);

	wl_resource_for_each(resource, &output->resources) {
		if (client == wl_resource_get_client(resource)) {
			wl_surface_send_enter(surface->resource, resource);
		}
	}
}

void wlr_surface_send_leave(struct wlr_surface *surface,
		struct wlr_output *output) {
	struct wl_client *client = wl_resource_get_client(surface->resource);
	struct wlr_surface_output *surface_output, *tmp;
	struct wl_resource *resource;

	wl_list_for_each_safe(surface_output, tmp,
			&surface->current_outputs, link) {
		if (surface_output->output == output) {
			surface_output_destroy(surface_output);
			wl_resource_for_each(resource, &output->resources) {
				if (client == wl_resource_get_client(resource)) {
					wl_surface_send_leave(surface->resource, resource);
				}
			}
			break;
		}
	}
}

void wlr_surface_send_frame_done(struct wlr_surface *surface,
		const struct timespec *when) {
	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp,
			&surface->current.frame_callback_list) {
		wl_callback_send_done(resource, timespec_to_msec(when));
		wl_resource_destroy(resource);
	}
}

static void surface_for_each_surface(struct wlr_surface *surface, int x, int y,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	struct wlr_subsurface *subsurface;
	wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link) {
		if (!subsurface->surface->mapped) {
			continue;
		}

		struct wlr_subsurface_parent_state *state = &subsurface->current;
		int sx = state->x;
		int sy = state->y;

		surface_for_each_surface(subsurface->surface, x + sx, y + sy,
			iterator, user_data);
	}

	iterator(surface, x, y, user_data);

	wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link) {
		if (!subsurface->surface->mapped) {
			continue;
		}

		struct wlr_subsurface_parent_state *state = &subsurface->current;
		int sx = state->x;
		int sy = state->y;

		surface_for_each_surface(subsurface->surface, x + sx, y + sy,
			iterator, user_data);
	}
}

void wlr_surface_for_each_surface(struct wlr_surface *surface,
		wlr_surface_iterator_func_t iterator, void *user_data) {
	surface_for_each_surface(surface, 0, 0, iterator, user_data);
}

struct bound_acc {
	int32_t min_x, min_y;
	int32_t max_x, max_y;
};

static void handle_bounding_box_surface(struct wlr_surface *surface,
		int x, int y, void *data) {
	struct bound_acc *acc = data;

	acc->min_x = min(x, acc->min_x);
	acc->min_y = min(y, acc->min_y);

	acc->max_x = max(x + surface->current.width, acc->max_x);
	acc->max_y = max(y + surface->current.height, acc->max_y);
}

void wlr_surface_get_extents(struct wlr_surface *surface, struct wlr_box *box) {
	struct bound_acc acc = {
		.min_x = 0,
		.min_y = 0,
		.max_x = surface->current.width,
		.max_y = surface->current.height,
	};

	wlr_surface_for_each_surface(surface, handle_bounding_box_surface, &acc);

	box->x = acc.min_x;
	box->y = acc.min_y;
	box->width = acc.max_x - acc.min_x;
	box->height = acc.max_y - acc.min_y;
}

static void crop_region(pixman_region32_t *dst, pixman_region32_t *src,
		const struct wlr_box *box) {
	pixman_region32_intersect_rect(dst, src,
		box->x, box->y, box->width, box->height);
	pixman_region32_translate(dst, -box->x, -box->y);
}

void wlr_surface_get_effective_damage(struct wlr_surface *surface,
		pixman_region32_t *damage) {
	pixman_region32_clear(damage);

	// Transform and copy the buffer damage in terms of surface coordinates.
	wlr_region_transform(damage, &surface->buffer_damage,
		surface->current.transform, surface->current.buffer_width,
		surface->current.buffer_height);
	wlr_region_scale(damage, damage, 1.0 / (float)surface->current.scale);

	if (surface->current.viewport.has_src) {
		struct wlr_box src_box = {
			.x = floor(surface->current.viewport.src.x),
			.y = floor(surface->current.viewport.src.y),
			.width = ceil(surface->current.viewport.src.width),
			.height = ceil(surface->current.viewport.src.height),
		};
		crop_region(damage, damage, &src_box);
	}
	if (surface->current.viewport.has_dst) {
		int src_width, src_height;
		surface_state_viewport_src_size(&surface->current,
			&src_width, &src_height);
		float scale_x = (float)surface->current.viewport.dst_width / src_width;
		float scale_y = (float)surface->current.viewport.dst_height / src_height;
		wlr_region_scale_xy(damage, damage, scale_x, scale_y);
	}
}

void wlr_surface_get_buffer_source_box(struct wlr_surface *surface,
		struct wlr_fbox *box) {
	box->x = box->y = 0;
	box->width = surface->current.buffer_width;
	box->height = surface->current.buffer_height;

	if (surface->current.viewport.has_src) {
		box->x = surface->current.viewport.src.x * surface->current.scale;
		box->y = surface->current.viewport.src.y * surface->current.scale;
		box->width = surface->current.viewport.src.width * surface->current.scale;
		box->height = surface->current.viewport.src.height * surface->current.scale;

		int width, height;
		surface_state_transformed_buffer_size(&surface->current, &width, &height);
		wlr_fbox_transform(box, box,
			wlr_output_transform_invert(surface->current.transform),
			width, height);
	}
}

void wlr_surface_set_preferred_buffer_scale(struct wlr_surface *surface,
		int32_t scale) {
	assert(scale > 0);

	if (wl_resource_get_version(surface->resource) <
			WL_SURFACE_PREFERRED_BUFFER_SCALE_SINCE_VERSION) {
		return;
	}

	if (surface->preferred_buffer_scale == scale) {
		return;
	}

	wl_surface_send_preferred_buffer_scale(surface->resource, scale);
	surface->preferred_buffer_scale = scale;
}

void wlr_surface_set_preferred_buffer_transform(struct wlr_surface *surface,
		enum wl_output_transform transform) {
	if (wl_resource_get_version(surface->resource) <
			WL_SURFACE_PREFERRED_BUFFER_TRANSFORM_SINCE_VERSION) {
		return;
	}

	if (surface->preferred_buffer_transform == transform &&
			surface->preferred_buffer_transform_sent) {
		return;
	}

	wl_surface_send_preferred_buffer_transform(surface->resource, transform);
	surface->preferred_buffer_transform_sent = true;
	surface->preferred_buffer_transform = transform;
}

static const struct wl_compositor_interface compositor_impl;

static struct wlr_compositor *compositor_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wl_compositor_interface,
		&compositor_impl));
	return wl_resource_get_user_data(resource);
}

static void compositor_create_surface(struct wl_client *client,
		struct wl_resource *resource, uint32_t id) {
	struct wlr_compositor *compositor = compositor_from_resource(resource);

	struct wlr_surface *surface = surface_create(client,
		wl_resource_get_version(resource), id, compositor);
	if (surface == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_signal_emit_mutable(&compositor->events.new_surface, surface);
}

static void compositor_create_region(struct wl_client *client,
		struct wl_resource *resource, uint32_t id) {
	region_create(client, wl_resource_get_version(resource), id);
}

static const struct wl_compositor_interface compositor_impl = {
	.create_surface = compositor_create_surface,
	.create_region = compositor_create_region,
};

static void compositor_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_compositor *compositor = data;

	struct wl_resource *resource =
		wl_resource_create(wl_client, &wl_compositor_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	wl_resource_set_implementation(resource, &compositor_impl, compositor, NULL);
}

static void compositor_handle_display_destroy(
		struct wl_listener *listener, void *data) {
	struct wlr_compositor *compositor =
		wl_container_of(listener, compositor, display_destroy);
	wl_signal_emit_mutable(&compositor->events.destroy, NULL);

	assert(wl_list_empty(&compositor->events.new_surface.listener_list));
	assert(wl_list_empty(&compositor->events.destroy.listener_list));

	wl_list_remove(&compositor->display_destroy.link);
	wl_list_remove(&compositor->renderer_destroy.link);
	wl_global_destroy(compositor->global);
	free(compositor);
}

static void compositor_handle_renderer_destroy(
		struct wl_listener *listener, void *data) {
	struct wlr_compositor *compositor =
		wl_container_of(listener, compositor, renderer_destroy);
	wlr_compositor_set_renderer(compositor, NULL);
}

struct wlr_compositor *wlr_compositor_create(struct wl_display *display,
		uint32_t version, struct wlr_renderer *renderer) {
	assert(version <= COMPOSITOR_VERSION);

	struct wlr_compositor *compositor = calloc(1, sizeof(*compositor));
	if (!compositor) {
		return NULL;
	}

	compositor->global = wl_global_create(display, &wl_compositor_interface,
		version, compositor, compositor_bind);
	if (!compositor->global) {
		free(compositor);
		return NULL;
	}

	wl_signal_init(&compositor->events.new_surface);
	wl_signal_init(&compositor->events.destroy);

	wl_list_init(&compositor->renderer_destroy.link);

	compositor->display_destroy.notify = compositor_handle_display_destroy;
	wl_display_add_destroy_listener(display, &compositor->display_destroy);

	wlr_compositor_set_renderer(compositor, renderer);

	return compositor;
}

void wlr_compositor_set_renderer(struct wlr_compositor *compositor,
		struct wlr_renderer *renderer) {
	wl_list_remove(&compositor->renderer_destroy.link);
	compositor->renderer = renderer;

	if (renderer != NULL) {
		compositor->renderer_destroy.notify = compositor_handle_renderer_destroy;
		wl_signal_add(&renderer->events.destroy, &compositor->renderer_destroy);
	} else {
		wl_list_init(&compositor->renderer_destroy.link);
	}
}

static bool surface_state_add_synced(struct wlr_surface_state *state, void *value) {
	void **ptr = wl_array_add(&state->synced, sizeof(void *));
	if (ptr == NULL) {
		return false;
	}
	*ptr = value;
	return true;
}

static void *surface_state_remove_synced(struct wlr_surface_state *state,
		struct wlr_surface_synced *synced) {
	void **synced_states = state->synced.data;
	void *synced_state = synced_states[synced->index];
	array_remove_at(&state->synced, synced->index * sizeof(void *), sizeof(void *));
	return synced_state;
}

static void surface_state_remove_and_destroy_synced(struct wlr_surface_state *state,
		struct wlr_surface_synced *synced) {
	void *synced_state = surface_state_remove_synced(state, synced);
	surface_synced_destroy_state(synced, synced_state);
}

bool wlr_surface_synced_init(struct wlr_surface_synced *synced,
		struct wlr_surface *surface, const struct wlr_surface_synced_impl *impl,
		void *pending, void *current) {
	assert(impl->state_size > 0);

	struct wlr_surface_synced *other;
	wl_list_for_each(other, &surface->synced, link) {
		assert(synced != other);
	}

	memset(pending, 0, impl->state_size);
	memset(current, 0, impl->state_size);
	if (impl->init_state) {
		impl->init_state(pending);
		impl->init_state(current);
	}
	if (!surface_state_add_synced(&surface->pending, pending)) {
		goto error_init;
	}
	if (!surface_state_add_synced(&surface->current, current)) {
		goto error_pending;
	}

	*synced = (struct wlr_surface_synced){
		.surface = surface,
		.impl = impl,
		.index = surface->synced_len,
	};

	struct wlr_surface_state *cached;
	wl_list_for_each(cached, &surface->cached, cached_state_link) {
		void *synced_state = surface_synced_create_state(synced);
		if (synced_state == NULL ||
				!surface_state_add_synced(cached, synced_state)) {
			surface_synced_destroy_state(synced, synced_state);
			goto error_cached;
		}
	}

	wl_list_insert(&surface->synced, &synced->link);
	surface->synced_len++;

	return true;

error_cached:;
	struct wlr_surface_state *failed_at = cached;
	wl_list_for_each(cached, &surface->cached, cached_state_link) {
		if (cached == failed_at) {
			break;
		}
		surface_state_remove_and_destroy_synced(cached, synced);
	}
	surface_state_remove_synced(&surface->current, synced);
error_pending:
	surface_state_remove_synced(&surface->pending, synced);
error_init:
	if (synced->impl->finish_state) {
		synced->impl->finish_state(pending);
		synced->impl->finish_state(current);
	}
	return false;
}

void wlr_surface_synced_finish(struct wlr_surface_synced *synced) {
	struct wlr_surface *surface = synced->surface;

	bool found = false;
	struct wlr_surface_synced *other;
	wl_list_for_each(other, &surface->synced, link) {
		if (other == synced) {
			found = true;
		} else if (other->index > synced->index) {
			other->index--;
		}
	}
	assert(found);

	struct wlr_surface_state *cached;
	wl_list_for_each(cached, &surface->cached, cached_state_link) {
		surface_state_remove_and_destroy_synced(cached, synced);
	}

	void *pending = surface_state_remove_synced(&surface->pending, synced);
	void *current = surface_state_remove_synced(&surface->current, synced);
	if (synced->impl->finish_state) {
		synced->impl->finish_state(pending);
		synced->impl->finish_state(current);
	}

	wl_list_remove(&synced->link);
	synced->surface->synced_len--;
}

void *wlr_surface_synced_get_state(struct wlr_surface_synced *synced,
		const struct wlr_surface_state *state) {
	void **synced_states = state->synced.data;
	return synced_states[synced->index];
}
