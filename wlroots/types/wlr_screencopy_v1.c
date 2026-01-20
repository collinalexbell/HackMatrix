#include <assert.h>
#include <stdlib.h>
#include <drm_fourcc.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/backend.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <wlr/util/transform.h>
#include "wlr-screencopy-unstable-v1-protocol.h"
#include "render/pixel_format.h"
#include "render/wlr_renderer.h"

#define SCREENCOPY_MANAGER_VERSION 3

struct screencopy_damage {
	struct wl_list link;
	struct wlr_output *output;
	struct pixman_region32 damage;
	struct wl_listener output_precommit;
	struct wl_listener output_destroy;
};

static const struct zwlr_screencopy_frame_v1_interface frame_impl;

static struct screencopy_damage *screencopy_damage_find(
		struct wlr_screencopy_v1_client *client,
		struct wlr_output *output) {
	struct screencopy_damage *damage;

	wl_list_for_each(damage, &client->damages, link) {
		if (damage->output == output) {
			return damage;
		}
	}

	return NULL;
}

static void screencopy_damage_accumulate(struct screencopy_damage *damage,
		const struct wlr_output_state *state) {
	struct pixman_region32 *region = &damage->damage;
	struct wlr_output *output = damage->output;

	if (state->committed & WLR_OUTPUT_STATE_DAMAGE) {
		// If the compositor submitted damage, copy it over
		pixman_region32_union(region, region, &state->damage);
		pixman_region32_intersect_rect(region, region, 0, 0,
			output->width, output->height);
	} else if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		// If the compositor did not submit damage but did submit a buffer
		// damage everything
		pixman_region32_union_rect(region, region, 0, 0,
			output->width, output->height);
	}
}

static void screencopy_damage_handle_output_precommit(
		struct wl_listener *listener, void *data) {
	struct screencopy_damage *damage =
		wl_container_of(listener, damage, output_precommit);
	const struct wlr_output_event_precommit *event = data;
	screencopy_damage_accumulate(damage, event->state);
}

static void screencopy_damage_destroy(struct screencopy_damage *damage) {
	wl_list_remove(&damage->output_destroy.link);
	wl_list_remove(&damage->output_precommit.link);
	wl_list_remove(&damage->link);
	pixman_region32_fini(&damage->damage);
	free(damage);
}

static void screencopy_damage_handle_output_destroy(
		struct wl_listener *listener, void *data) {
	struct screencopy_damage *damage =
		wl_container_of(listener, damage, output_destroy);
	screencopy_damage_destroy(damage);
}

static struct screencopy_damage *screencopy_damage_create(
		struct wlr_screencopy_v1_client *client,
		struct wlr_output *output) {
	struct screencopy_damage *damage = calloc(1, sizeof(*damage));
	if (!damage) {
		return NULL;
	}

	damage->output = output;
	pixman_region32_init_rect(&damage->damage, 0, 0, output->width,
		output->height);
	wl_list_insert(&client->damages, &damage->link);

	wl_signal_add(&output->events.precommit, &damage->output_precommit);
	damage->output_precommit.notify =
		screencopy_damage_handle_output_precommit;

	wl_signal_add(&output->events.destroy, &damage->output_destroy);
	damage->output_destroy.notify = screencopy_damage_handle_output_destroy;

	return damage;
}

static struct screencopy_damage *screencopy_damage_get_or_create(
		struct wlr_screencopy_v1_client *client,
		struct wlr_output *output) {
	struct screencopy_damage *damage = screencopy_damage_find(client, output);
	return damage ? damage : screencopy_damage_create(client, output);
}

static void client_unref(struct wlr_screencopy_v1_client *client) {
	assert(client->ref > 0);

	if (--client->ref != 0) {
		return;
	}

	struct screencopy_damage *damage, *tmp_damage;
	wl_list_for_each_safe(damage, tmp_damage, &client->damages, link) {
		screencopy_damage_destroy(damage);
	}

	free(client);
}

static struct wlr_screencopy_frame_v1 *frame_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&zwlr_screencopy_frame_v1_interface, &frame_impl));
	return wl_resource_get_user_data(resource);
}

static void frame_destroy(struct wlr_screencopy_frame_v1 *frame) {
	if (frame == NULL) {
		return;
	}
	if (frame->output != NULL && frame->buffer != NULL) {
		wlr_output_lock_attach_render(frame->output, false);
		if (frame->cursor_locked) {
			wlr_output_lock_software_cursors(frame->output, false);
		}
	}
	wl_list_remove(&frame->link);
	wl_list_remove(&frame->output_commit.link);
	wl_list_remove(&frame->output_destroy.link);
	// Make the frame resource inert
	wl_resource_set_user_data(frame->resource, NULL);
	wlr_buffer_unlock(frame->buffer);
	client_unref(frame->client);
	free(frame);
}

static void frame_send_damage(struct wlr_screencopy_frame_v1 *frame) {
	if (!frame->with_damage) {
		return;
	}

	struct screencopy_damage *damage =
		screencopy_damage_get_or_create(frame->client, frame->output);
	if (damage == NULL) {
		return;
	}

	int n_boxes;
	const pixman_box32_t *boxes = pixman_region32_rectangles(&damage->damage, &n_boxes);
	for (int i = 0; i < n_boxes; i++) {
		const pixman_box32_t *box = &boxes[i];

		int damage_x = box->x1;
		int damage_y = box->y1;
		int damage_width = box->x2 - box->x1;
		int damage_height = box->y2 - box->y1;

		zwlr_screencopy_frame_v1_send_damage(frame->resource,
			damage_x, damage_y, damage_width, damage_height);
	}

	pixman_region32_clear(&damage->damage);
}

static void frame_send_ready(struct wlr_screencopy_frame_v1 *frame,
		struct timespec *when) {
	time_t tv_sec = when->tv_sec;
	uint32_t tv_sec_hi = (sizeof(tv_sec) > 4) ? tv_sec >> 32 : 0;
	uint32_t tv_sec_lo = tv_sec & 0xFFFFFFFF;
	zwlr_screencopy_frame_v1_send_ready(frame->resource,
		tv_sec_hi, tv_sec_lo, when->tv_nsec);
}

static bool frame_shm_copy(struct wlr_screencopy_frame_v1 *frame,
		struct wlr_buffer *src_buffer) {
	struct wlr_output *output = frame->output;
	struct wlr_renderer *renderer = output->renderer;
	assert(renderer);

	void *data;
	uint32_t format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(frame->buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride)) {
		return false;
	}

	bool ok = false;

	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, src_buffer);
	if (!texture) {
		wlr_log(WLR_DEBUG, "Failed to grab a texture from a buffer during shm screencopy");
		goto out;
	}

	ok = wlr_texture_read_pixels(texture, &(struct wlr_texture_read_pixels_options) {
		.data = data,
		.format = format,
		.stride = stride,
		.src_box = frame->box,
	});

	wlr_texture_destroy(texture);

out:
	wlr_buffer_end_data_ptr_access(frame->buffer);

	if (!ok) {
		wlr_log(WLR_DEBUG, "Failed to copy to destination during shm screencopy");
	}

	return ok;
}

static bool frame_dma_copy(struct wlr_screencopy_frame_v1 *frame,
		struct wlr_buffer *src_buffer) {
	struct wlr_buffer *dst_buffer = frame->buffer;
	struct wlr_output *output = frame->output;
	struct wlr_renderer *renderer = output->renderer;
	assert(renderer);

	struct wlr_texture *src_tex =
		wlr_texture_from_buffer(renderer, src_buffer);
	if (src_tex == NULL) {
		wlr_log(WLR_DEBUG, "Failed to grab a texture from a buffer during dma screencopy");
		return false;
	}

	bool ok = false;

	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(renderer, dst_buffer, NULL);
	if (!pass) {
		goto out;
	}

	wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options) {
		.texture = src_tex,
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
		.dst_box = (struct wlr_box){
			.width = dst_buffer->width,
			.height = dst_buffer->height,
		},
		.src_box = (struct wlr_fbox){
			.x = frame->box.x,
			.y = frame->box.y,
			.width = frame->box.width,
			.height = frame->box.height,
		},
	});

	ok = wlr_render_pass_submit(pass);

out:
	wlr_texture_destroy(src_tex);

	if (!ok) {
		wlr_log(WLR_DEBUG, "Failed to render to destination during dma screencopy");
	}

	return ok;
}

static void frame_handle_output_commit(struct wl_listener *listener,
		void *data) {
	struct wlr_screencopy_frame_v1 *frame =
		wl_container_of(listener, frame, output_commit);
	struct wlr_output_event_commit *event = data;
	struct wlr_output *output = frame->output;

	if (event->state->committed & WLR_OUTPUT_STATE_ENABLED && !output->enabled) {
		goto err;
	}

	if (!(event->state->committed & WLR_OUTPUT_STATE_BUFFER)) {
		return;
	}

	if (!frame->buffer) {
		return;
	}

	if (frame->with_damage) {
		struct screencopy_damage *damage =
			screencopy_damage_get_or_create(frame->client, output);
		if (damage && pixman_region32_empty(&damage->damage)) {
			return;
		}
	}

	wl_list_remove(&frame->output_commit.link);
	wl_list_init(&frame->output_commit.link);

	struct wlr_buffer *src_buffer = event->state->buffer;
	if (frame->box.x < 0 || frame->box.y < 0 ||
			frame->box.x + frame->box.width > src_buffer->width ||
			frame->box.y + frame->box.height > src_buffer->height) {
		goto err;
	}

	switch (frame->buffer_cap) {
	case WLR_BUFFER_CAP_DMABUF:
		if (!frame_dma_copy(frame, src_buffer)) {
			goto err;
		}
		break;
	case WLR_BUFFER_CAP_DATA_PTR:
		if (!frame_shm_copy(frame, src_buffer)) {
			goto err;
		}
		break;
	default:
		abort(); // unreachable
	}

	zwlr_screencopy_frame_v1_send_flags(frame->resource, 0);
	frame_send_damage(frame);
	frame_send_ready(frame, &event->when);
	frame_destroy(frame);
	return;

err:
	zwlr_screencopy_frame_v1_send_failed(frame->resource);
	frame_destroy(frame);
}

static void frame_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_screencopy_frame_v1 *frame =
		wl_container_of(listener, frame, output_destroy);
	zwlr_screencopy_frame_v1_send_failed(frame->resource);
	frame_destroy(frame);
}

static void frame_handle_copy(struct wl_client *wl_client,
		struct wl_resource *frame_resource,
		struct wl_resource *buffer_resource) {
	struct wlr_screencopy_frame_v1 *frame = frame_from_resource(frame_resource);
	if (frame == NULL) {
		return;
	}

	struct wlr_output *output = frame->output;

	if (!output->enabled) {
		zwlr_screencopy_frame_v1_send_failed(frame->resource);
		frame_destroy(frame);
		return;
	}

	struct wlr_buffer *buffer = wlr_buffer_try_from_resource(buffer_resource);
	if (buffer == NULL) {
		wl_resource_post_error(frame->resource,
			ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
			"invalid buffer");
		return;
	}

	if (buffer->width != frame->box.width || buffer->height != frame->box.height) {
		wl_resource_post_error(frame->resource,
			ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
			"invalid buffer dimensions");
		return;
	}

	if (frame->buffer != NULL) {
		wl_resource_post_error(frame->resource,
			ZWLR_SCREENCOPY_FRAME_V1_ERROR_ALREADY_USED,
			"frame already used");
		return;
	}

	enum wlr_buffer_cap cap;
	struct wlr_dmabuf_attributes dmabuf;
	void *data;
	uint32_t format;
	size_t stride;
	if (wlr_buffer_get_dmabuf(buffer, &dmabuf)) {
		cap = WLR_BUFFER_CAP_DMABUF;

		if (dmabuf.format != frame->dmabuf_format) {
			wl_resource_post_error(frame->resource,
				ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
				"invalid buffer format");
			return;
		}
	} else if (wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride)) {
		wlr_buffer_end_data_ptr_access(buffer);

		cap = WLR_BUFFER_CAP_DATA_PTR;

		if (format != frame->shm_format) {
			wl_resource_post_error(frame->resource,
				ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
				"invalid buffer format");
			return;
		}
		if (stride != (size_t)frame->shm_stride) {
			wl_resource_post_error(frame->resource,
				ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
				"invalid buffer stride");
			return;
		}
	} else {
		wl_resource_post_error(frame->resource,
			ZWLR_SCREENCOPY_FRAME_V1_ERROR_INVALID_BUFFER,
			"unsupported buffer type");
		return;
	}

	frame->buffer = buffer;
	frame->buffer_cap = cap;

	wl_signal_add(&output->events.commit, &frame->output_commit);
	frame->output_commit.notify = frame_handle_output_commit;

	// Request a frame because we can't assume that the current front buffer is still usable. It may
	// have been released already, and we shouldn't lock it here because compositors want to render
	// into the least damaged buffer.
	wlr_output_update_needs_frame(output);

	wlr_output_lock_attach_render(output, true);
	if (frame->overlay_cursor) {
		wlr_output_lock_software_cursors(output, true);
		frame->cursor_locked = true;
	}
}

static void frame_handle_copy_with_damage(struct wl_client *wl_client,
		struct wl_resource *frame_resource,
		struct wl_resource *buffer_resource) {
	struct wlr_screencopy_frame_v1 *frame = frame_from_resource(frame_resource);
	if (frame == NULL) {
		return;
	}
	frame->with_damage = true;
	frame_handle_copy(wl_client, frame_resource, buffer_resource);
}

static void frame_handle_destroy(struct wl_client *wl_client,
		struct wl_resource *frame_resource) {
	wl_resource_destroy(frame_resource);
}

static const struct zwlr_screencopy_frame_v1_interface frame_impl = {
	.copy = frame_handle_copy,
	.destroy = frame_handle_destroy,
	.copy_with_damage = frame_handle_copy_with_damage,
};

static void frame_handle_resource_destroy(struct wl_resource *frame_resource) {
	struct wlr_screencopy_frame_v1 *frame = frame_from_resource(frame_resource);
	frame_destroy(frame);
}


static const struct zwlr_screencopy_manager_v1_interface manager_impl;

static struct wlr_screencopy_v1_client *client_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&zwlr_screencopy_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

static void capture_output(struct wl_client *wl_client,
		struct wlr_screencopy_v1_client *client, uint32_t version,
		uint32_t id, int32_t overlay_cursor, struct wlr_output *output,
		const struct wlr_box *box) {
	struct wlr_screencopy_frame_v1 *frame = calloc(1, sizeof(*frame));
	if (frame == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	frame->output = output;
	frame->overlay_cursor = !!overlay_cursor;

	frame->resource = wl_resource_create(wl_client,
		&zwlr_screencopy_frame_v1_interface, version, id);
	if (frame->resource == NULL) {
		free(frame);
		wl_client_post_no_memory(wl_client);
		return;
	}
	wl_resource_set_implementation(frame->resource, &frame_impl, frame,
		frame_handle_resource_destroy);

	if (output == NULL) {
		wl_resource_set_user_data(frame->resource, NULL);
		zwlr_screencopy_frame_v1_send_failed(frame->resource);
		free(frame);
		return;
	}

	frame->client = client;
	client->ref++;

	wl_list_insert(&client->manager->frames, &frame->link);

	wl_list_init(&frame->output_commit.link);

	wl_signal_add(&output->events.destroy, &frame->output_destroy);
	frame->output_destroy.notify = frame_handle_output_destroy;

	if (output == NULL || !output->enabled) {
		goto error;
	}

	struct wlr_renderer *renderer = output->renderer;
	assert(renderer);

	if (!wlr_output_configure_primary_swapchain(output, NULL, &output->swapchain)) {
		goto error;
	}

	struct wlr_buffer *buffer = wlr_swapchain_acquire(output->swapchain);
	if (buffer == NULL) {
		goto error;
	}

	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, buffer);
	wlr_buffer_unlock(buffer);
	if (!texture) {
		goto error;
	}

	frame->shm_format = wlr_texture_preferred_read_format(texture);
	wlr_texture_destroy(texture);

	if (frame->shm_format == DRM_FORMAT_INVALID) {
		wlr_log(WLR_ERROR,
			"Failed to capture output: no read format supported by renderer");
		goto error;
	}
	const struct wlr_pixel_format_info *shm_info =
		drm_get_pixel_format_info(frame->shm_format);
	if (!shm_info) {
		wlr_log(WLR_ERROR,
			"Failed to capture output: no pixel format info matching read format");
		goto error;
	}

	if (output->allocator &&
			(output->allocator->buffer_caps & WLR_BUFFER_CAP_DMABUF)) {
		frame->dmabuf_format = output->render_format;
	} else {
		frame->dmabuf_format = DRM_FORMAT_INVALID;
	}

	struct wlr_box buffer_box = {0};
	if (box == NULL) {
		buffer_box.width = output->width;
		buffer_box.height = output->height;
	} else {
		int ow, oh;
		wlr_output_effective_resolution(output, &ow, &oh);

		buffer_box = *box;

		wlr_box_transform(&buffer_box, &buffer_box,
			wlr_output_transform_invert(output->transform), ow, oh);
		buffer_box.x *= output->scale;
		buffer_box.y *= output->scale;
		buffer_box.width *= output->scale;
		buffer_box.height *= output->scale;
	}

	frame->box = buffer_box;
	frame->shm_stride = pixel_format_info_min_stride(shm_info, buffer_box.width);

	zwlr_screencopy_frame_v1_send_buffer(frame->resource,
		convert_drm_format_to_wl_shm(frame->shm_format),
		buffer_box.width, buffer_box.height, frame->shm_stride);

	if (version >= 3) {
		if (frame->dmabuf_format != DRM_FORMAT_INVALID) {
			zwlr_screencopy_frame_v1_send_linux_dmabuf(
					frame->resource, frame->dmabuf_format,
					buffer_box.width, buffer_box.height);
		}

		zwlr_screencopy_frame_v1_send_buffer_done(frame->resource);
	}

	return;

error:
	zwlr_screencopy_frame_v1_send_failed(frame->resource);
	frame_destroy(frame);
}

static void manager_handle_capture_output(struct wl_client *wl_client,
		struct wl_resource *manager_resource, uint32_t id,
		int32_t overlay_cursor, struct wl_resource *output_resource) {
	struct wlr_screencopy_v1_client *client =
		client_from_resource(manager_resource);
	uint32_t version = wl_resource_get_version(manager_resource);
	struct wlr_output *output = wlr_output_from_resource(output_resource);

	capture_output(wl_client, client, version, id, overlay_cursor, output,
		NULL);
}

static void manager_handle_capture_output_region(struct wl_client *wl_client,
		struct wl_resource *manager_resource, uint32_t id,
		int32_t overlay_cursor, struct wl_resource *output_resource,
		int32_t x, int32_t y, int32_t width, int32_t height) {
	struct wlr_screencopy_v1_client *client =
		client_from_resource(manager_resource);
	uint32_t version = wl_resource_get_version(manager_resource);
	struct wlr_output *output = wlr_output_from_resource(output_resource);

	struct wlr_box box = {
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};
	capture_output(wl_client, client, version, id, overlay_cursor, output,
		&box);
}

static void manager_handle_destroy(struct wl_client *wl_client,
		struct wl_resource *manager_resource) {
	wl_resource_destroy(manager_resource);
}

static const struct zwlr_screencopy_manager_v1_interface manager_impl = {
	.capture_output = manager_handle_capture_output,
	.capture_output_region = manager_handle_capture_output_region,
	.destroy = manager_handle_destroy,
};

static void manager_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_screencopy_v1_client *client =
		client_from_resource(resource);
	client_unref(client);
}

static void manager_bind(struct wl_client *wl_client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_screencopy_manager_v1 *manager = data;

	struct wlr_screencopy_v1_client *client = calloc(1, sizeof(*client));
	if (client == NULL) {
		goto failure;
	}

	struct wl_resource *resource = wl_resource_create(wl_client,
		&zwlr_screencopy_manager_v1_interface, version, id);
	if (resource == NULL) {
		goto failure;
	}

	client->ref = 1;
	client->manager = manager;
	wl_list_init(&client->damages);

	wl_resource_set_implementation(resource, &manager_impl, client,
		manager_handle_resource_destroy);

	return;
failure:
	free(client);
	wl_client_post_no_memory(wl_client);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_screencopy_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_signal_emit_mutable(&manager->events.destroy, manager);

	assert(wl_list_empty(&manager->events.destroy.listener_list));

	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wlr_screencopy_manager_v1 *wlr_screencopy_manager_v1_create(
		struct wl_display *display) {
	struct wlr_screencopy_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&zwlr_screencopy_manager_v1_interface, SCREENCOPY_MANAGER_VERSION,
		manager, manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}
	wl_list_init(&manager->frames);

	wl_signal_init(&manager->events.destroy);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}
