#include <assert.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include "types/wlr_buffer.h"

static const struct wlr_buffer_impl client_buffer_impl;

struct wlr_client_buffer *wlr_client_buffer_get(struct wlr_buffer *wlr_buffer) {
	if (wlr_buffer->impl != &client_buffer_impl) {
		return NULL;
	}
	struct wlr_client_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	return buffer;
}

static struct wlr_client_buffer *client_buffer_from_buffer(
		struct wlr_buffer *buffer) {
	struct wlr_client_buffer *client_buffer = wlr_client_buffer_get(buffer);
	assert(client_buffer != NULL);
	return client_buffer;
}

static void client_buffer_destroy(struct wlr_buffer *buffer) {
	struct wlr_client_buffer *client_buffer = client_buffer_from_buffer(buffer);

	wlr_buffer_finish(buffer);

	wl_list_remove(&client_buffer->source_destroy.link);
	wl_list_remove(&client_buffer->renderer_destroy.link);
	wlr_texture_destroy(client_buffer->texture);
	free(client_buffer);
}

static bool client_buffer_get_dmabuf(struct wlr_buffer *buffer,
		struct wlr_dmabuf_attributes *attribs) {
	struct wlr_client_buffer *client_buffer = client_buffer_from_buffer(buffer);

	if (client_buffer->source == NULL) {
		return false;
	}

	return wlr_buffer_get_dmabuf(client_buffer->source, attribs);
}

static bool client_buffer_get_shm(struct wlr_buffer *buffer,
		struct wlr_shm_attributes *attribs) {
	struct wlr_client_buffer *client_buffer = client_buffer_from_buffer(buffer);

	if (client_buffer->source == NULL) {
		return false;
	}

	return wlr_buffer_get_shm(client_buffer->source, attribs);
}

static bool client_buffer_begin_data_ptr_access(struct wlr_buffer *buffer, uint32_t flags,
		void **data, uint32_t *format, size_t *stride) {
	struct wlr_client_buffer *client_buffer = client_buffer_from_buffer(buffer);

	if (client_buffer->source == NULL) {
		return false;
	}

	return wlr_buffer_begin_data_ptr_access(client_buffer->source, flags, data, format, stride);
}

static void client_buffer_end_data_ptr_access(struct wlr_buffer *buffer) {
	struct wlr_client_buffer *client_buffer = client_buffer_from_buffer(buffer);

	if (client_buffer->source == NULL) {
		return;
	}

	wlr_buffer_end_data_ptr_access(client_buffer->source);
}

static const struct wlr_buffer_impl client_buffer_impl = {
	.destroy = client_buffer_destroy,
	.get_dmabuf = client_buffer_get_dmabuf,
	.get_shm = client_buffer_get_shm,
	.begin_data_ptr_access = client_buffer_begin_data_ptr_access,
	.end_data_ptr_access = client_buffer_end_data_ptr_access,
};

static void client_buffer_handle_source_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_client_buffer *client_buffer =
		wl_container_of(listener, client_buffer, source_destroy);
	wl_list_remove(&client_buffer->source_destroy.link);
	wl_list_init(&client_buffer->source_destroy.link);
	client_buffer->source = NULL;
}

static void client_buffer_handle_renderer_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_client_buffer *client_buffer =
		wl_container_of(listener, client_buffer, renderer_destroy);
	wl_list_remove(&client_buffer->renderer_destroy.link);
	wl_list_init(&client_buffer->renderer_destroy.link);
	client_buffer->texture = NULL;
}

struct wlr_client_buffer *wlr_client_buffer_create(struct wlr_buffer *buffer,
		struct wlr_renderer *renderer) {
	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, buffer);
	if (texture == NULL) {
		wlr_log(WLR_ERROR, "Failed to create texture");
		return NULL;
	}

	struct wlr_client_buffer *client_buffer = calloc(1, sizeof(*client_buffer));
	if (client_buffer == NULL) {
		wlr_texture_destroy(texture);
		return NULL;
	}
	wlr_buffer_init(&client_buffer->base, &client_buffer_impl,
		texture->width, texture->height);
	client_buffer->source = buffer;
	client_buffer->texture = texture;

	wl_signal_add(&buffer->events.destroy, &client_buffer->source_destroy);
	client_buffer->source_destroy.notify = client_buffer_handle_source_destroy;

	wl_signal_add(&texture->renderer->events.destroy, &client_buffer->renderer_destroy);
	client_buffer->renderer_destroy.notify = client_buffer_handle_renderer_destroy;

	// Ensure the buffer will be released before being destroyed
	wlr_buffer_lock(&client_buffer->base);
	wlr_buffer_drop(&client_buffer->base);

	return client_buffer;
}

bool wlr_client_buffer_apply_damage(struct wlr_client_buffer *client_buffer,
		struct wlr_buffer *next, const pixman_region32_t *damage) {
	if (client_buffer->base.n_locks - client_buffer->n_ignore_locks > 1) {
		// Someone else still has a reference to the buffer
		return false;
	}
	if (client_buffer->texture == NULL) {
		return false;
	}

	return wlr_texture_update_from_buffer(client_buffer->texture, next, damage);
}
