#include <assert.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include "single-pixel-buffer-v1-protocol.h"

#define SINGLE_PIXEL_MANAGER_VERSION 1

static void destroy_resource(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wl_buffer_interface wl_buffer_impl = {
	.destroy = destroy_resource,
};

static const struct wlr_buffer_impl buffer_impl;

static bool buffer_resource_is_instance(struct wl_resource *resource) {
	return wl_resource_instance_of(resource, &wl_buffer_interface,
		&wl_buffer_impl);
}

static struct wlr_single_pixel_buffer_v1 *single_pixel_buffer_v1_from_resource(
		struct wl_resource *resource) {
	assert(buffer_resource_is_instance(resource));
	return wl_resource_get_user_data(resource);
}

static struct wlr_buffer *buffer_from_resource(
		struct wl_resource *resource) {
	return &single_pixel_buffer_v1_from_resource(resource)->base;
}

static const struct wlr_buffer_resource_interface buffer_resource_interface = {
	.name = "single_pixel_buffer_v1",
	.is_instance = buffer_resource_is_instance,
	.from_resource = buffer_from_resource,
};

static void buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct wlr_single_pixel_buffer_v1 *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	wl_list_remove(&buffer->release.link);

	wlr_buffer_finish(wlr_buffer);

	if (buffer->resource != NULL) {
		wl_resource_set_user_data(buffer->resource, NULL);
	}
	free(buffer);
}

static bool buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct wlr_single_pixel_buffer_v1 *buffer =
		wl_container_of(wlr_buffer, buffer, base);
	if (flags & ~WLR_BUFFER_DATA_PTR_ACCESS_READ) {
		return false; // the buffer is read-only
	}
	*data = &buffer->argb8888;
	*format = DRM_FORMAT_ARGB8888;
	*stride = sizeof(buffer->argb8888);
	return true;
}

static void buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	// This space is intentionally left blank
}

static const struct wlr_buffer_impl buffer_impl = {
	.destroy = buffer_destroy,
	.begin_data_ptr_access = buffer_begin_data_ptr_access,
	.end_data_ptr_access = buffer_end_data_ptr_access,
};

static void buffer_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_single_pixel_buffer_v1 *buffer = single_pixel_buffer_v1_from_resource(resource);
	buffer->resource = NULL;
	wlr_buffer_drop(&buffer->base);
}

static void buffer_handle_release(struct wl_listener *listener, void *data) {
	struct wlr_single_pixel_buffer_v1 *buffer = wl_container_of(listener, buffer, release);
	if (buffer->resource != NULL) {
		wl_buffer_send_release(buffer->resource);
	}
}

static void manager_handle_create_u32_rgba_buffer(struct wl_client *client,
		struct wl_resource *resource, uint32_t id, uint32_t r, uint32_t g,
		uint32_t b, uint32_t a) {
	struct wlr_single_pixel_buffer_v1 *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	buffer->resource = wl_resource_create(client, &wl_buffer_interface, 1, id);
	if (buffer->resource == NULL) {
		wl_client_post_no_memory(client);
		free(buffer);
		return;
	}

	wlr_buffer_init(&buffer->base, &buffer_impl, 1, 1);
	wl_resource_set_implementation(buffer->resource,
		&wl_buffer_impl, buffer, buffer_handle_resource_destroy);

	buffer->r = r;
	buffer->g = g;
	buffer->b = b;
	buffer->a = a;

	double f = (double)0xFF / 0xFFFFFFFF;
	buffer->argb8888[0] = (uint8_t)((double)buffer->b * f);
	buffer->argb8888[1] = (uint8_t)((double)buffer->g * f);
	buffer->argb8888[2] = (uint8_t)((double)buffer->r * f);
	buffer->argb8888[3] = (uint8_t)((double)buffer->a * f);

	buffer->release.notify = buffer_handle_release;
	wl_signal_add(&buffer->base.events.release, &buffer->release);
}

static const struct wp_single_pixel_buffer_manager_v1_interface manager_impl = {
	.destroy = destroy_resource,
	.create_u32_rgba_buffer = manager_handle_create_u32_rgba_buffer,
};

static void manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(client,
		&wp_single_pixel_buffer_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_single_pixel_buffer_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wlr_single_pixel_buffer_manager_v1 *wlr_single_pixel_buffer_manager_v1_create(
		struct wl_display *display) {
	struct wlr_single_pixel_buffer_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&wp_single_pixel_buffer_manager_v1_interface,
		SINGLE_PIXEL_MANAGER_VERSION,
		NULL, manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	wlr_buffer_register_resource_interface(&buffer_resource_interface);

	return manager;
}

struct wlr_single_pixel_buffer_v1 *wlr_single_pixel_buffer_v1_try_from_buffer(
		struct wlr_buffer *buffer) {

	if (buffer->impl != &buffer_impl) {
		return NULL;
	}

	return wl_container_of(buffer,
		(struct wlr_single_pixel_buffer_v1 *)NULL, base);
}
