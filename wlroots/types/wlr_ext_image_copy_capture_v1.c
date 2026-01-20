#include <assert.h>
#include <pixman.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/render/wlr_renderer.h>
#include "ext-image-copy-capture-v1-protocol.h"
#include "render/pixel_format.h"

#define IMAGE_COPY_CAPTURE_MANAGER_V1_VERSION 1

struct wlr_ext_image_copy_capture_session_v1 {
	struct wl_resource *resource;
	struct wlr_ext_image_capture_source_v1 *source;
	struct wlr_ext_image_copy_capture_frame_v1 *frame;

	struct wl_listener source_destroy;
	struct wl_listener source_constraints_update;
	struct wl_listener source_frame;

	pixman_region32_t damage;
};

struct wlr_ext_image_copy_capture_cursor_session_v1 {
	struct wl_resource *resource;
	struct wlr_ext_image_capture_source_v1_cursor *source;
	bool capture_session_created;

	struct {
		bool entered;
		int32_t x, y;
		struct {
			int32_t x, y;
		} hotspot;
	} prev;

	struct wl_listener source_destroy;
	struct wl_listener source_update;
};

static const struct ext_image_copy_capture_frame_v1_interface frame_impl;
static const struct ext_image_copy_capture_session_v1_interface session_impl;
static const struct ext_image_copy_capture_cursor_session_v1_interface cursor_session_impl;

static struct wlr_ext_image_copy_capture_frame_v1 *frame_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_image_copy_capture_frame_v1_interface, &frame_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_ext_image_copy_capture_session_v1 *session_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_image_copy_capture_session_v1_interface, &session_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_ext_image_copy_capture_cursor_session_v1 *cursor_session_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_image_copy_capture_cursor_session_v1_interface, &cursor_session_impl));
	return wl_resource_get_user_data(resource);
}

static void frame_destroy(struct wlr_ext_image_copy_capture_frame_v1 *frame) {
	if (frame == NULL) {
		return;
	}
	wl_signal_emit_mutable(&frame->events.destroy, NULL);
	assert(wl_list_empty(&frame->events.destroy.listener_list));
	wl_resource_set_user_data(frame->resource, NULL);
	wlr_buffer_unlock(frame->buffer);
	pixman_region32_fini(&frame->buffer_damage);
	if (frame->session->frame == frame) {
		frame->session->frame = NULL;
	}
	free(frame);
}

static void frame_handle_resource_destroy(struct wl_resource *resource) {
	frame_destroy(wl_resource_get_user_data(resource));
}

void wlr_ext_image_copy_capture_frame_v1_ready(struct wlr_ext_image_copy_capture_frame_v1 *frame,
		enum wl_output_transform transform,
		const struct timespec *presentation_time) {
	assert(frame->capturing);

	int rects_len = 0;
	const pixman_box32_t *rects =
		pixman_region32_rectangles(&frame->session->damage, &rects_len);
	for (int i = 0; i < rects_len; i++) {
		const pixman_box32_t *rect = &rects[i];
		ext_image_copy_capture_frame_v1_send_damage(frame->resource,
			rect->x1, rect->y1, rect->x2 - rect->x1, rect->y2 - rect->y1);
	}

	pixman_region32_clear(&frame->session->damage);

	uint64_t pres_time_sec = (uint64_t)presentation_time->tv_sec;
	ext_image_copy_capture_frame_v1_send_transform(frame->resource, transform);
	ext_image_copy_capture_frame_v1_send_presentation_time(frame->resource,
		pres_time_sec >> 32, (uint32_t)pres_time_sec, presentation_time->tv_nsec);
	ext_image_copy_capture_frame_v1_send_ready(frame->resource);
	frame_destroy(frame);
}

static bool copy_dmabuf(struct wlr_buffer *dst,
		struct wlr_buffer *src, struct wlr_renderer *renderer,
		const pixman_region32_t *clip) {
	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, src);
	if (texture == NULL) {
		return false;
	}

	bool ok = false;
	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(renderer, dst, NULL);
	if (!pass) {
		goto out;
	}

	wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options) {
		.texture = texture,
		.clip = clip,
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});

	ok = wlr_render_pass_submit(pass);

out:
	wlr_texture_destroy(texture);
	return ok;
}

static bool copy_shm(void *data, uint32_t format, size_t stride,
		struct wlr_buffer *src, struct wlr_renderer *renderer) {
	// TODO: bypass renderer if source buffer supports data ptr access
	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, src);
	if (!texture) {
		return false;
	}

	// TODO: only copy damaged region
	bool ok = wlr_texture_read_pixels(texture, &(struct wlr_texture_read_pixels_options){
		.data = data,
		.format = format,
		.stride = stride,
	});

	wlr_texture_destroy(texture);

	return ok;
}

bool wlr_ext_image_copy_capture_frame_v1_copy_buffer(struct wlr_ext_image_copy_capture_frame_v1 *frame,
		struct wlr_buffer *src, struct wlr_renderer *renderer) {
	struct wlr_buffer *dst = frame->buffer;

	if (src->width != dst->width || src->height != dst->height) {
		wlr_ext_image_copy_capture_frame_v1_fail(frame,
			EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS);
		return false;
	}

	bool ok = false;
	enum ext_image_copy_capture_frame_v1_failure_reason failure_reason =
		EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_UNKNOWN;
	struct wlr_dmabuf_attributes dmabuf;
	void *data;
	uint32_t format;
	size_t stride;
	if (wlr_buffer_get_dmabuf(dst, &dmabuf)) {
		if (frame->session->source->dmabuf_formats.len == 0) {
			ok = false;
			failure_reason = EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS;
		} else {
			ok = copy_dmabuf(dst, src, renderer, &frame->buffer_damage);
		}
	} else if (wlr_buffer_begin_data_ptr_access(dst,
			WLR_BUFFER_DATA_PTR_ACCESS_WRITE, &data, &format, &stride)) {
		if (frame->session->source->shm_formats_len == 0) {
			ok = false;
			failure_reason = EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_BUFFER_CONSTRAINTS;
		} else {
			ok = copy_shm(data, format, stride, src, renderer);
		}
		wlr_buffer_end_data_ptr_access(dst);
	}
	if (!ok) {
		wlr_ext_image_copy_capture_frame_v1_fail(frame, failure_reason);
		return false;
	}

	return true;
}

void wlr_ext_image_copy_capture_frame_v1_fail(struct wlr_ext_image_copy_capture_frame_v1 *frame,
		enum ext_image_copy_capture_frame_v1_failure_reason reason) {
	ext_image_copy_capture_frame_v1_send_failed(frame->resource, reason);
	frame_destroy(frame);
}

static void frame_handle_destroy(struct wl_client *client,
		struct wl_resource *frame_resource) {
	wl_resource_destroy(frame_resource);
}

static void frame_handle_attach_buffer(struct wl_client *client,
		struct wl_resource *frame_resource, struct wl_resource *buffer_resource) {
	struct wlr_ext_image_copy_capture_frame_v1 *frame = frame_from_resource(frame_resource);
	if (frame == NULL) {
		return;
	}

	if (frame->capturing) {
		wl_resource_post_error(frame->resource,
			EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED,
			"attach_buffer sent after capture");
		return;
	}

	struct wlr_buffer *buffer = wlr_buffer_try_from_resource(buffer_resource);
	if (buffer == NULL) {
		wl_resource_post_no_memory(frame_resource);
		return;
	}

	wlr_buffer_unlock(frame->buffer);
	frame->buffer = buffer;
}

static void frame_handle_damage_buffer(struct wl_client *client,
		struct wl_resource *frame_resource, int32_t x, int32_t y,
		int32_t width, int32_t height) {
	struct wlr_ext_image_copy_capture_frame_v1 *frame = frame_from_resource(frame_resource);
	if (frame == NULL) {
		return;
	}

	if (frame->capturing) {
		wl_resource_post_error(frame->resource,
			EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED,
			"damage_buffer sent after capture");
		return;
	}

	if (x < 0 || y < 0 || width <= 0 || height <= 0) {
		wl_resource_post_error(frame->resource,
			EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_INVALID_BUFFER_DAMAGE,
			"Invalid buffer damage coordinates");
		return;
	}

	pixman_region32_union_rect(&frame->buffer_damage, &frame->buffer_damage,
		x, y, width, height);
}

static void frame_handle_capture(struct wl_client *client,
		struct wl_resource *frame_resource) {
	struct wlr_ext_image_copy_capture_frame_v1 *frame = frame_from_resource(frame_resource);
	if (frame == NULL) {
		return;
	}

	if (frame->capturing) {
		wl_resource_post_error(frame->resource,
			EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_ALREADY_CAPTURED,
			"capture sent twice");
		return;
	}

	if (frame->buffer == NULL) {
		wl_resource_post_error(frame->resource,
			EXT_IMAGE_COPY_CAPTURE_FRAME_V1_ERROR_NO_BUFFER,
			"No buffer attached");
		return;
	}

	frame->capturing = true;

	bool need_frame = !pixman_region32_empty(&frame->session->damage);
	struct wlr_ext_image_capture_source_v1 *source = frame->session->source;
	if (need_frame && source->impl->schedule_frame) {
		source->impl->schedule_frame(source);
	}
}

static const struct ext_image_copy_capture_frame_v1_interface frame_impl = {
	.destroy = frame_handle_destroy,
	.attach_buffer = frame_handle_attach_buffer,
	.damage_buffer = frame_handle_damage_buffer,
	.capture = frame_handle_capture,
};

static void session_handle_destroy(struct wl_client *client,
		struct wl_resource *session_resource) {
	wl_resource_destroy(session_resource);
}

static void session_handle_create_frame(struct wl_client *client,
		struct wl_resource *session_resource, uint32_t new_id) {
	struct wlr_ext_image_copy_capture_session_v1 *session = session_from_resource(session_resource);

	if (session != NULL && session->frame != NULL) {
		wl_resource_post_error(session_resource,
			EXT_IMAGE_COPY_CAPTURE_SESSION_V1_ERROR_DUPLICATE_FRAME,
			"session already has a frame object");
		return;
	}

	uint32_t version = wl_resource_get_version(session_resource);
	struct wl_resource *frame_resource = wl_resource_create(client,
		&ext_image_copy_capture_frame_v1_interface, version, new_id);
	if (frame_resource == NULL) {
		wl_resource_post_no_memory(frame_resource);
		return;
	}
	wl_resource_set_implementation(frame_resource, &frame_impl, NULL,
		frame_handle_resource_destroy);

	if (session == NULL) {
		ext_image_copy_capture_frame_v1_send_failed(frame_resource,
			EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED);
		return;
	}

	struct wlr_ext_image_copy_capture_frame_v1 *frame = calloc(1, sizeof(*frame));
	if (frame == NULL) {
		wl_resource_post_no_memory(session_resource);
		return;
	}

	frame->resource = frame_resource;
	frame->session = session;
	pixman_region32_init(&frame->buffer_damage);
	wl_signal_init(&frame->events.destroy);

	wl_resource_set_user_data(frame_resource, frame);

	session->frame = frame;
}

static const struct ext_image_copy_capture_session_v1_interface session_impl = {
	.destroy = session_handle_destroy,
	.create_frame = session_handle_create_frame,
};

static void session_send_constraints(struct wlr_ext_image_copy_capture_session_v1 *session) {
	struct wlr_ext_image_capture_source_v1 *source = session->source;

	ext_image_copy_capture_session_v1_send_buffer_size(session->resource,
		source->width, source->height);

	for (size_t i = 0; i < source->shm_formats_len; i++) {
		ext_image_copy_capture_session_v1_send_shm_format(session->resource,
			convert_drm_format_to_wl_shm(source->shm_formats[i]));
	}

	if (source->dmabuf_formats.len > 0) {
		struct wl_array dev_id_array = {
			.data = &source->dmabuf_device,
			.size = sizeof(source->dmabuf_device),
		};
		ext_image_copy_capture_session_v1_send_dmabuf_device(session->resource,
			&dev_id_array);
	}
	for (size_t i = 0; i < source->dmabuf_formats.len; i++) {
		struct wlr_drm_format *fmt = &source->dmabuf_formats.formats[i];
		struct wl_array modifiers_array = {
			.data = fmt->modifiers,
			.size = fmt->len * sizeof(fmt->modifiers[0]),
		};
		ext_image_copy_capture_session_v1_send_dmabuf_format(session->resource,
			fmt->format, &modifiers_array);
	}

	ext_image_copy_capture_session_v1_send_done(session->resource);
}

static void session_destroy(struct wlr_ext_image_copy_capture_session_v1 *session) {
	if (session == NULL) {
		return;
	}

	if (session->frame != NULL) {
		wlr_ext_image_copy_capture_frame_v1_fail(session->frame,
			EXT_IMAGE_COPY_CAPTURE_FRAME_V1_FAILURE_REASON_STOPPED);
	}

	if (session->source->impl->stop) {
		session->source->impl->stop(session->source);
	}

	ext_image_copy_capture_session_v1_send_stopped(session->resource);
	wl_resource_set_user_data(session->resource, NULL);

	pixman_region32_fini(&session->damage);
	wl_list_remove(&session->source_destroy.link);
	wl_list_remove(&session->source_constraints_update.link);
	wl_list_remove(&session->source_frame.link);
	free(session);
}

static void session_handle_source_destroy(struct wl_listener *listener, void *data) {
	struct wlr_ext_image_copy_capture_session_v1 *session = wl_container_of(listener, session, source_destroy);
	session_destroy(session);
}

static void session_handle_source_constraints_update(struct wl_listener *listener,
		void *data) {
	struct wlr_ext_image_copy_capture_session_v1 *session =
		wl_container_of(listener, session, source_constraints_update);
	session_send_constraints(session);
}

static void session_handle_source_frame(struct wl_listener *listener, void *data) {
	struct wlr_ext_image_copy_capture_session_v1 *session = wl_container_of(listener, session, source_frame);
	struct wlr_ext_image_capture_source_v1_frame_event *event = data;

	pixman_region32_union(&session->damage, &session->damage, event->damage);

	struct wlr_ext_image_copy_capture_frame_v1 *frame = session->frame;
	if (frame != NULL && frame->capturing &&
			!pixman_region32_empty(&session->damage)) {
		pixman_region32_union(&frame->buffer_damage,
			&frame->buffer_damage, &session->damage);

		struct wlr_ext_image_capture_source_v1 *source = frame->session->source;
		source->impl->copy_frame(source, frame, event);
	}
}

static void session_handle_resource_destroy(struct wl_resource *resource) {
	session_destroy(session_from_resource(resource));
}

static void session_create(struct wl_resource *parent_resource, uint32_t new_id,
		struct wlr_ext_image_capture_source_v1 *source, uint32_t options) {
	struct wl_client *client = wl_resource_get_client(parent_resource);
	uint32_t version = wl_resource_get_version(parent_resource);
	struct wl_resource *session_resource = wl_resource_create(client,
		&ext_image_copy_capture_session_v1_interface, version, new_id);
	if (session_resource == NULL) {
		wl_resource_post_no_memory(parent_resource);
		return;
	}
	wl_resource_set_implementation(session_resource, &session_impl, NULL,
		session_handle_resource_destroy);

	if (source == NULL) {
		ext_image_copy_capture_session_v1_send_stopped(session_resource);
		return;
	}

	struct wlr_ext_image_copy_capture_session_v1 *session = calloc(1, sizeof(*session));
	if (session == NULL) {
		wl_resource_post_no_memory(parent_resource);
		return;
	}

	if (source->impl->start) {
		source->impl->start(source, options & EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS);
	}

	session->resource = session_resource;
	session->source = source;
	pixman_region32_init_rect(&session->damage, 0, 0, source->width,
			source->height);

	session->source_destroy.notify = session_handle_source_destroy;
	wl_signal_add(&source->events.destroy, &session->source_destroy);

	session->source_constraints_update.notify = session_handle_source_constraints_update;
	wl_signal_add(&source->events.constraints_update, &session->source_constraints_update);

	session->source_frame.notify = session_handle_source_frame;
	wl_signal_add(&source->events.frame, &session->source_frame);

	wl_resource_set_user_data(session_resource, session);
	session_send_constraints(session);
}

static void cursor_session_destroy(struct wlr_ext_image_copy_capture_cursor_session_v1 *cursor_session) {
	if (cursor_session == NULL) {
		return;
	}
	if (cursor_session->source->entered) {
		ext_image_copy_capture_cursor_session_v1_send_leave(cursor_session->resource);
	}
	wl_resource_set_user_data(cursor_session->resource, NULL);
	wl_list_remove(&cursor_session->source_destroy.link);
	wl_list_remove(&cursor_session->source_update.link);
	free(cursor_session);
}

static void cursor_session_handle_destroy(struct wl_client *client,
		struct wl_resource *cursor_session_resource) {
	wl_resource_destroy(cursor_session_resource);
}

static void cursor_session_handle_get_capture_session(struct wl_client *client,
		struct wl_resource *cursor_session_resource, uint32_t new_id) {
	struct wlr_ext_image_copy_capture_cursor_session_v1 *cursor_session =
		cursor_session_from_resource(cursor_session_resource);

	if (cursor_session != NULL && cursor_session->capture_session_created) {
		wl_resource_post_error(cursor_session_resource,
			EXT_IMAGE_COPY_CAPTURE_CURSOR_SESSION_V1_ERROR_DUPLICATE_SESSION,
			"get_capture_session sent twice");
		return;
	}

	cursor_session->capture_session_created = true;

	struct wlr_ext_image_capture_source_v1 *source = NULL;
	if (cursor_session != NULL) {
		source = &cursor_session->source->base;
	}

	session_create(cursor_session_resource, new_id, source, 0);
}

static const struct ext_image_copy_capture_cursor_session_v1_interface cursor_session_impl = {
	.destroy = cursor_session_handle_destroy,
	.get_capture_session = cursor_session_handle_get_capture_session,
};

static void cursor_session_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_ext_image_copy_capture_cursor_session_v1 *cursor_session =
		cursor_session_from_resource(resource);
	cursor_session_destroy(cursor_session);
}

static void cursor_session_handle_source_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_ext_image_copy_capture_cursor_session_v1 *cursor_session =
		wl_container_of(listener, cursor_session, source_destroy);
	cursor_session_destroy(cursor_session);
}

static void cursor_session_update(
		struct wlr_ext_image_copy_capture_cursor_session_v1 *cursor_session) {
	struct wlr_ext_image_capture_source_v1_cursor *cursor_source = cursor_session->source;

	if (cursor_source->entered && !cursor_session->prev.entered) {
		ext_image_copy_capture_cursor_session_v1_send_enter(cursor_session->resource);
	}
	if (!cursor_source->entered && cursor_session->prev.entered) {
		ext_image_copy_capture_cursor_session_v1_send_leave(cursor_session->resource);
	}

	if (cursor_source->x != cursor_session->prev.x ||
			cursor_source->y != cursor_session->prev.y) {
		ext_image_copy_capture_cursor_session_v1_send_position(cursor_session->resource,
			cursor_source->x, cursor_source->y);
	}

	if (cursor_source->hotspot.x != cursor_session->prev.hotspot.x ||
			cursor_source->hotspot.y != cursor_session->prev.hotspot.y) {
		ext_image_copy_capture_cursor_session_v1_send_hotspot(cursor_session->resource,
			cursor_source->hotspot.x, cursor_source->hotspot.y);
	}

	cursor_session->prev.entered = cursor_source->entered;
	cursor_session->prev.x = cursor_source->x;
	cursor_session->prev.y = cursor_source->y;
	cursor_session->prev.hotspot.y = cursor_source->hotspot.y;
	cursor_session->prev.hotspot.y = cursor_source->hotspot.y;
}

static void cursor_session_handle_source_update(struct wl_listener *listener,
		void *data) {
	struct wlr_ext_image_copy_capture_cursor_session_v1 *cursor_session =
		wl_container_of(listener, cursor_session, source_update);
	cursor_session_update(cursor_session);
}

static void manager_handle_create_session(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t new_id,
		struct wl_resource *source_resource, uint32_t options) {
	struct wlr_ext_image_capture_source_v1 *source =
		wlr_ext_image_capture_source_v1_from_resource(source_resource);
	session_create(manager_resource, new_id, source, options);
}

static void manager_handle_create_pointer_cursor_session(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t new_id,
		struct wl_resource *source_resource, struct wl_resource *pointer_resource) {
	struct wlr_ext_image_capture_source_v1 *source = wlr_ext_image_capture_source_v1_from_resource(source_resource);
	struct wlr_seat_client *seat_client = wlr_seat_client_from_pointer_resource(pointer_resource);

	struct wlr_seat *seat = NULL;
	if (seat_client != NULL) {
		seat = seat_client->seat;
	}

	struct wlr_ext_image_capture_source_v1_cursor *source_cursor = NULL;
	if (source != NULL && seat != NULL && source->impl->get_pointer_cursor) {
		source_cursor = source->impl->get_pointer_cursor(source, seat);
	}

	uint32_t version = wl_resource_get_version(manager_resource);
	struct wl_resource *cursor_session_resource = wl_resource_create(client,
		&ext_image_copy_capture_cursor_session_v1_interface, version, new_id);
	if (cursor_session_resource == NULL) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}
	wl_resource_set_implementation(cursor_session_resource, &cursor_session_impl, NULL,
		cursor_session_handle_resource_destroy);

	if (source_cursor == NULL) {
		return; // leave inert
	}

	struct wlr_ext_image_copy_capture_cursor_session_v1 *cursor_session = calloc(1, sizeof(*cursor_session));
	if (cursor_session == NULL) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	cursor_session->resource = cursor_session_resource;
	cursor_session->source = source_cursor;

	cursor_session->source_destroy.notify = cursor_session_handle_source_destroy;
	wl_signal_add(&source_cursor->base.events.destroy, &cursor_session->source_destroy);

	cursor_session->source_update.notify = cursor_session_handle_source_update;
	wl_signal_add(&source_cursor->events.update, &cursor_session->source_update);

	wl_resource_set_user_data(cursor_session_resource, cursor_session);

	cursor_session_update(cursor_session);
}

static void manager_handle_destroy(struct wl_client *client,
		struct wl_resource *manager_resource) {
	wl_resource_destroy(manager_resource);
}

static const struct ext_image_copy_capture_manager_v1_interface manager_impl = {
	.create_session = manager_handle_create_session,
	.create_pointer_cursor_session = manager_handle_create_pointer_cursor_session,
	.destroy = manager_handle_destroy,
};

static void manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(client,
		&ext_image_copy_capture_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}

static void manager_handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_ext_image_copy_capture_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wlr_ext_image_copy_capture_manager_v1 *wlr_ext_image_copy_capture_manager_v1_create(
		struct wl_display *display, uint32_t version) {
	assert(version <= IMAGE_COPY_CAPTURE_MANAGER_V1_VERSION);

	struct wlr_ext_image_copy_capture_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&ext_image_copy_capture_manager_v1_interface, version, manager, manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	manager->display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}
