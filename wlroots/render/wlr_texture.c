#include <assert.h>
#include <drm_fourcc.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/render/interface.h>
#include <wlr/render/wlr_texture.h>
#include "render/pixel_format.h"
#include "types/wlr_buffer.h"

void wlr_texture_init(struct wlr_texture *texture, struct wlr_renderer *renderer,
		const struct wlr_texture_impl *impl, uint32_t width, uint32_t height) {
	assert(renderer);

	*texture = (struct wlr_texture){
		.renderer = renderer,
		.impl = impl,
		.width = width,
		.height = height,
	};
}

void wlr_texture_destroy(struct wlr_texture *texture) {
	if (texture && texture->impl && texture->impl->destroy) {
		texture->impl->destroy(texture);
	} else {
		free(texture);
	}
}

void wlr_texture_read_pixels_options_get_src_box(
		const struct wlr_texture_read_pixels_options *options,
		const struct wlr_texture *texture, struct wlr_box *box) {
	if (wlr_box_empty(&options->src_box)) {
		*box = (struct wlr_box){
			.x = 0,
			.y = 0,
			.width = texture->width,
			.height = texture->height,
		};
		return;
	}

	*box = options->src_box;
}

void *wlr_texture_read_pixel_options_get_data(
		const struct wlr_texture_read_pixels_options *options) {
	const struct wlr_pixel_format_info *fmt = drm_get_pixel_format_info(options->format);

	return (char *)options->data +
		pixel_format_info_min_stride(fmt, options->dst_x) +
		options->dst_y * options->stride;
}

bool wlr_texture_read_pixels(struct wlr_texture *texture,
		const struct wlr_texture_read_pixels_options *options) {
	if (!texture->impl->read_pixels) {
		return false;
	}

	return texture->impl->read_pixels(texture, options);
}

uint32_t wlr_texture_preferred_read_format(struct wlr_texture *texture) {
	if (!texture->impl->preferred_read_format) {
		return DRM_FORMAT_INVALID;
	}

	return texture->impl->preferred_read_format(texture);
}

struct wlr_texture *wlr_texture_from_pixels(struct wlr_renderer *renderer,
		uint32_t fmt, uint32_t stride, uint32_t width, uint32_t height,
		const void *data) {
	assert(width > 0);
	assert(height > 0);
	assert(stride > 0);
	assert(data);

	struct wlr_readonly_data_buffer *buffer =
		readonly_data_buffer_create(fmt, stride, width, height, data);
	if (buffer == NULL) {
		return NULL;
	}

	struct wlr_texture *texture =
		wlr_texture_from_buffer(renderer, &buffer->base);

	// By this point, the renderer should have locked the buffer if it still
	// needs to access it in the future.
	readonly_data_buffer_drop(buffer);

	return texture;
}

struct wlr_texture *wlr_texture_from_dmabuf(struct wlr_renderer *renderer,
		struct wlr_dmabuf_attributes *attribs) {
	struct wlr_dmabuf_buffer *buffer = dmabuf_buffer_create(attribs);
	if (buffer == NULL) {
		return NULL;
	}

	struct wlr_texture *texture =
		wlr_texture_from_buffer(renderer, &buffer->base);

	// By this point, the renderer should have locked the buffer if it still
	// needs to access it in the future.
	dmabuf_buffer_drop(buffer);

	return texture;
}

struct wlr_texture *wlr_texture_from_buffer(struct wlr_renderer *renderer,
		struct wlr_buffer *buffer) {
	if (!renderer->impl->texture_from_buffer) {
		return NULL;
	}
	return renderer->impl->texture_from_buffer(renderer, buffer);
}

bool wlr_texture_update_from_buffer(struct wlr_texture *texture,
		struct wlr_buffer *buffer, const pixman_region32_t *damage) {
	if (!texture->impl->update_from_buffer) {
		return false;
	}
	if (texture->width != (uint32_t)buffer->width ||
			texture->height != (uint32_t)buffer->height) {
		return false;
	}
	const pixman_box32_t *extents = pixman_region32_extents(damage);
	if (extents->x1 < 0 || extents->y1 < 0 || extents->x2 > buffer->width ||
			extents->y2 > buffer->height) {
		return false;
	}
	return texture->impl->update_from_buffer(texture, buffer, damage);
}
