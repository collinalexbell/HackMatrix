#include <assert.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>
#include <wlr/util/log.h>
#include "ext-image-capture-source-v1-protocol.h"
#include "render/wlr_renderer.h"

static void source_handle_destroy(struct wl_client *client,
		struct wl_resource *source_resource) {
	wl_resource_destroy(source_resource);
}

static const struct ext_image_capture_source_v1_interface source_impl = {
	.destroy = source_handle_destroy,
};

struct wlr_ext_image_capture_source_v1 *wlr_ext_image_capture_source_v1_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_image_capture_source_v1_interface, &source_impl));
	return wl_resource_get_user_data(resource);
}

static void source_handle_resource_destroy(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

void wlr_ext_image_capture_source_v1_init(struct wlr_ext_image_capture_source_v1 *source,
		const struct wlr_ext_image_capture_source_v1_interface *impl) {
	*source = (struct wlr_ext_image_capture_source_v1){
		.impl = impl,
	};
	wl_list_init(&source->resources);
	wl_signal_init(&source->events.destroy);
	wl_signal_init(&source->events.constraints_update);
	wl_signal_init(&source->events.frame);
}

void wlr_ext_image_capture_source_v1_finish(struct wlr_ext_image_capture_source_v1 *source) {
	wl_signal_emit_mutable(&source->events.destroy, NULL);

	assert(wl_list_empty(&source->events.destroy.listener_list));
	assert(wl_list_empty(&source->events.constraints_update.listener_list));
	assert(wl_list_empty(&source->events.frame.listener_list));

	struct wl_resource *resource, *resource_tmp;
	wl_resource_for_each_safe(resource, resource_tmp, &source->resources) {
		wl_resource_set_user_data(resource, NULL);
		wl_list_remove(wl_resource_get_link(resource));
		wl_list_init(wl_resource_get_link(resource));
	}

	free(source->shm_formats);
	wlr_drm_format_set_finish(&source->dmabuf_formats);
}

bool wlr_ext_image_capture_source_v1_create_resource(struct wlr_ext_image_capture_source_v1 *source,
		struct wl_client *client, uint32_t new_id) {
	struct wl_resource *resource = wl_resource_create(client,
		&ext_image_capture_source_v1_interface, 1, new_id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return false;
	}
	wl_resource_set_implementation(resource, &source_impl, source,
		source_handle_resource_destroy);
	if (source != NULL) {
		wl_list_insert(&source->resources, wl_resource_get_link(resource));
	} else {
		wl_list_init(wl_resource_get_link(resource));
	}
	return true;
}

static uint32_t get_swapchain_shm_format(struct wlr_swapchain *swapchain,
		struct wlr_renderer *renderer) {
	struct wlr_buffer *buffer = wlr_swapchain_acquire(swapchain);
	if (buffer == NULL) {
		return DRM_FORMAT_INVALID;
	}

	struct wlr_texture *texture = wlr_texture_from_buffer(renderer, buffer);
	wlr_buffer_unlock(buffer);
	if (texture == NULL) {
		return DRM_FORMAT_INVALID;
	}

	uint32_t format = wlr_texture_preferred_read_format(texture);
	wlr_texture_destroy(texture);

	return format;
}

static void add_drm_format(struct wlr_drm_format_set *set, const struct wlr_drm_format *fmt) {
	for (size_t i = 0; i < fmt->len; i++) {
		wlr_drm_format_set_add(set, fmt->format, fmt->modifiers[i]);
	}
}

bool wlr_ext_image_capture_source_v1_set_constraints_from_swapchain(struct wlr_ext_image_capture_source_v1 *source,
		struct wlr_swapchain *swapchain, struct wlr_renderer *renderer) {
	source->width = swapchain->width;
	source->height = swapchain->height;

	uint32_t shm_format = get_swapchain_shm_format(swapchain, renderer);
	if (shm_format != DRM_FORMAT_INVALID) {
		uint32_t *shm_formats = calloc(1, sizeof(shm_formats[0]));
		if (shm_formats == NULL) {
			wlr_log(WLR_ERROR, "Allocation failed");
			return false;
		}
		shm_formats[0] = shm_format;

		source->shm_formats_len = 1;
		free(source->shm_formats);
		source->shm_formats = shm_formats;
	}

	int drm_fd = wlr_renderer_get_drm_fd(renderer);
	if (swapchain->allocator != NULL &&
			(swapchain->allocator->buffer_caps & WLR_BUFFER_CAP_DMABUF) &&
			drm_fd >= 0) {
		struct stat dev_stat;
		if (fstat(drm_fd, &dev_stat) != 0) {
			wlr_log_errno(WLR_ERROR, "fstat() failed");
			return false;
		}

		source->dmabuf_device = dev_stat.st_rdev;

		wlr_drm_format_set_finish(&source->dmabuf_formats);
		source->dmabuf_formats = (struct wlr_drm_format_set){0};

		add_drm_format(&source->dmabuf_formats, &swapchain->format);

		const struct wlr_drm_format_set *render_formats =
			wlr_renderer_get_render_formats(renderer);
		assert(render_formats != NULL);

		// Not all clients support fancy formats. Always ensure we provide
		// support for ARGB8888 and XRGB8888 for simple clients.
		uint32_t fallback_formats[] = { DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888 };
		for (size_t i = 0; i < sizeof(fallback_formats) / sizeof(fallback_formats[0]); i++) {
			const struct wlr_drm_format *fmt =
				wlr_drm_format_set_get(render_formats, fallback_formats[i]);
			if (fmt != NULL && swapchain->format.format != fmt->format) {
				add_drm_format(&source->dmabuf_formats, fmt);
			}
		}
	}

	wl_signal_emit_mutable(&source->events.constraints_update, NULL);
	return true;
}

void wlr_ext_image_capture_source_v1_cursor_init(struct wlr_ext_image_capture_source_v1_cursor *source_cursor,
		const struct wlr_ext_image_capture_source_v1_interface *impl) {
	*source_cursor = (struct wlr_ext_image_capture_source_v1_cursor){0};
	wlr_ext_image_capture_source_v1_init(&source_cursor->base, impl);
	wl_signal_init(&source_cursor->events.update);
}

void wlr_ext_image_capture_source_v1_cursor_finish(struct wlr_ext_image_capture_source_v1_cursor *source_cursor) {
	wlr_ext_image_capture_source_v1_finish(&source_cursor->base);
	assert(wl_list_empty(&source_cursor->events.update.listener_list));
}
