#include <assert.h>
#include <drm_fourcc.h>
#include <pixman.h>
#include <stdlib.h>
#include <wayland-util.h>
#include <wlr/render/interface.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>

#include "render/pixman.h"
#include "types/wlr_buffer.h"

static const struct wlr_renderer_impl renderer_impl;

bool wlr_renderer_is_pixman(struct wlr_renderer *wlr_renderer) {
	return wlr_renderer->impl == &renderer_impl;
}

static struct wlr_pixman_renderer *get_renderer(
		struct wlr_renderer *wlr_renderer) {
	assert(wlr_renderer_is_pixman(wlr_renderer));
	struct wlr_pixman_renderer *renderer = wl_container_of(wlr_renderer, renderer, wlr_renderer);
	return renderer;
}

bool begin_pixman_data_ptr_access(struct wlr_buffer *wlr_buffer, pixman_image_t **image_ptr,
		uint32_t flags) {
	pixman_image_t *image = *image_ptr;

	void *data = NULL;
	uint32_t drm_format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(wlr_buffer, flags,
			&data, &drm_format, &stride)) {
		return false;
	}

	// If the data pointer has changed, re-create the Pixman image. This can
	// happen if it's a client buffer and the wl_shm_pool has been resized.
	if (data != pixman_image_get_data(image)) {
		pixman_format_code_t format = get_pixman_format_from_drm(drm_format);
		assert(format != 0);

		pixman_image_t *new_image = pixman_image_create_bits_no_clear(format,
			wlr_buffer->width, wlr_buffer->height, data, stride);
		if (new_image == NULL) {
			wlr_buffer_end_data_ptr_access(wlr_buffer);
			return false;
		}

		pixman_image_unref(image);
		image = new_image;
	}

	*image_ptr = image;
	return true;
}

static struct wlr_pixman_buffer *get_buffer(
		struct wlr_pixman_renderer *renderer, struct wlr_buffer *wlr_buffer) {
	struct wlr_pixman_buffer *buffer;
	wl_list_for_each(buffer, &renderer->buffers, link) {
		if (buffer->buffer == wlr_buffer) {
			return buffer;
		}
	}
	return NULL;
}

static const struct wlr_texture_impl texture_impl;

bool wlr_texture_is_pixman(struct wlr_texture *texture) {
	return texture->impl == &texture_impl;
}

static struct wlr_pixman_texture *get_texture(
		struct wlr_texture *wlr_texture) {
	assert(wlr_texture_is_pixman(wlr_texture));
	struct wlr_pixman_texture *texture = wl_container_of(wlr_texture, texture, wlr_texture);
	return texture;
}

static void texture_destroy(struct wlr_texture *wlr_texture) {
	struct wlr_pixman_texture *texture = get_texture(wlr_texture);
	wl_list_remove(&texture->link);
	pixman_image_unref(texture->image);
	wlr_buffer_unlock(texture->buffer);
	free(texture->data);
	free(texture);
}

static bool texture_read_pixels(struct wlr_texture *wlr_texture,
		const struct wlr_texture_read_pixels_options *options) {
	struct wlr_pixman_texture *texture = get_texture(wlr_texture);

	struct wlr_box src;
	wlr_texture_read_pixels_options_get_src_box(options, wlr_texture, &src);

	pixman_format_code_t fmt = get_pixman_format_from_drm(options->format);
	if (fmt == 0) {
		wlr_log(WLR_ERROR, "Cannot read pixels: unsupported pixel format");
		return false;
	}

	void *p = wlr_texture_read_pixel_options_get_data(options);

	pixman_image_t *dst = pixman_image_create_bits_no_clear(fmt,
			src.width, src.height, p, options->stride);

	pixman_image_composite32(PIXMAN_OP_SRC, texture->image, NULL, dst,
			src.x, src.y, 0, 0, 0, 0, src.width, src.height);

	pixman_image_unref(dst);

	return true;
}

static uint32_t pixman_texture_preferred_read_format(struct wlr_texture *wlr_texture) {
	struct wlr_pixman_texture *texture = get_texture(wlr_texture);

	pixman_format_code_t pixman_format = pixman_image_get_format(texture->image);
	return get_drm_format_from_pixman(pixman_format);
}

static const struct wlr_texture_impl texture_impl = {
	.read_pixels = texture_read_pixels,
	.preferred_read_format = pixman_texture_preferred_read_format,
	.destroy = texture_destroy,
};

static void destroy_buffer(struct wlr_pixman_buffer *buffer) {
	wl_list_remove(&buffer->link);
	wl_list_remove(&buffer->buffer_destroy.link);

	pixman_image_unref(buffer->image);

	free(buffer);
}

static void handle_destroy_buffer(struct wl_listener *listener, void *data) {
	struct wlr_pixman_buffer *buffer =
		wl_container_of(listener, buffer, buffer_destroy);
	destroy_buffer(buffer);
}

static struct wlr_pixman_buffer *create_buffer(
		struct wlr_pixman_renderer *renderer, struct wlr_buffer *wlr_buffer) {
	struct wlr_pixman_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return NULL;
	}
	buffer->buffer = wlr_buffer;
	buffer->renderer = renderer;

	void *data = NULL;
	uint32_t drm_format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(wlr_buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ | WLR_BUFFER_DATA_PTR_ACCESS_WRITE,
			&data, &drm_format, &stride)) {
		wlr_log(WLR_ERROR, "Failed to get buffer data");
		goto error_buffer;
	}
	wlr_buffer_end_data_ptr_access(wlr_buffer);

	pixman_format_code_t format = get_pixman_format_from_drm(drm_format);
	if (format == 0) {
		wlr_log(WLR_ERROR, "Unsupported pixman drm format 0x%"PRIX32,
				drm_format);
		goto error_buffer;
	}

	buffer->image = pixman_image_create_bits(format, wlr_buffer->width,
			wlr_buffer->height, data, stride);
	if (!buffer->image) {
		wlr_log(WLR_ERROR, "Failed to allocate pixman image");
		goto error_buffer;
	}

	buffer->buffer_destroy.notify = handle_destroy_buffer;
	wl_signal_add(&wlr_buffer->events.destroy, &buffer->buffer_destroy);

	wl_list_insert(&renderer->buffers, &buffer->link);

	wlr_log(WLR_DEBUG, "Created pixman buffer %dx%d",
		wlr_buffer->width, wlr_buffer->height);

	return buffer;

error_buffer:
	free(buffer);
	return NULL;
}

static const struct wlr_drm_format_set *pixman_get_texture_formats(
		struct wlr_renderer *wlr_renderer, uint32_t buffer_caps) {
	struct wlr_pixman_renderer *renderer = get_renderer(wlr_renderer);
	if (buffer_caps & WLR_BUFFER_CAP_DATA_PTR) {
		return &renderer->drm_formats;
	} else {
		return NULL;
	}
}

static const struct wlr_drm_format_set *pixman_get_render_formats(
		struct wlr_renderer *wlr_renderer) {
	struct wlr_pixman_renderer *renderer = get_renderer(wlr_renderer);
	return &renderer->drm_formats;
}

static struct wlr_pixman_texture *pixman_texture_create(
		struct wlr_pixman_renderer *renderer, uint32_t drm_format,
		uint32_t width, uint32_t height) {
	struct wlr_pixman_texture *texture = calloc(1, sizeof(*texture));
	if (texture == NULL) {
		wlr_log_errno(WLR_ERROR, "Failed to allocate pixman texture");
		return NULL;
	}

	wlr_texture_init(&texture->wlr_texture, &renderer->wlr_renderer,
		&texture_impl, width, height);

	texture->format_info = drm_get_pixel_format_info(drm_format);
	if (!texture->format_info) {
		wlr_log(WLR_ERROR, "Unsupported drm format 0x%"PRIX32, drm_format);
		free(texture);
		return NULL;
	}

	texture->format = get_pixman_format_from_drm(drm_format);
	if (texture->format == 0) {
		wlr_log(WLR_ERROR, "Unsupported pixman drm format 0x%"PRIX32,
				drm_format);
		free(texture);
		return NULL;
	}

	wl_list_insert(&renderer->textures, &texture->link);

	return texture;
}

static struct wlr_texture *pixman_texture_from_buffer(
		struct wlr_renderer *wlr_renderer, struct wlr_buffer *buffer) {
	struct wlr_pixman_renderer *renderer = get_renderer(wlr_renderer);

	void *data = NULL;
	uint32_t drm_format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(buffer, WLR_BUFFER_DATA_PTR_ACCESS_READ,
			&data, &drm_format, &stride)) {
		return NULL;
	}
	wlr_buffer_end_data_ptr_access(buffer);

	struct wlr_pixman_texture *texture = pixman_texture_create(renderer,
		drm_format, buffer->width, buffer->height);
	if (texture == NULL) {
		return NULL;
	}

	texture->image = pixman_image_create_bits_no_clear(texture->format,
		buffer->width, buffer->height, data, stride);
	if (!texture->image) {
		wlr_log(WLR_ERROR, "Failed to create pixman image");
		wl_list_remove(&texture->link);
		free(texture);
		return NULL;
	}

	texture->buffer = wlr_buffer_lock(buffer);

	return &texture->wlr_texture;
}

static void pixman_destroy(struct wlr_renderer *wlr_renderer) {
	struct wlr_pixman_renderer *renderer = get_renderer(wlr_renderer);

	struct wlr_pixman_buffer *buffer, *buffer_tmp;
	wl_list_for_each_safe(buffer, buffer_tmp, &renderer->buffers, link) {
		destroy_buffer(buffer);
	}

	struct wlr_pixman_texture *tex, *tex_tmp;
	wl_list_for_each_safe(tex, tex_tmp, &renderer->textures, link) {
		wlr_texture_destroy(&tex->wlr_texture);
	}

	wlr_drm_format_set_finish(&renderer->drm_formats);

	free(renderer);
}

static struct wlr_render_pass *pixman_begin_buffer_pass(struct wlr_renderer *wlr_renderer,
		struct wlr_buffer *wlr_buffer, const struct wlr_buffer_pass_options *options) {
	struct wlr_pixman_renderer *renderer = get_renderer(wlr_renderer);

	struct wlr_pixman_buffer *buffer = get_buffer(renderer, wlr_buffer);
	if (buffer == NULL) {
		buffer = create_buffer(renderer, wlr_buffer);
	}
	if (buffer == NULL) {
		return NULL;
	}

	struct wlr_pixman_render_pass *pass = begin_pixman_render_pass(buffer);
	if (pass == NULL) {
		return NULL;
	}
	return &pass->base;
}

static const struct wlr_renderer_impl renderer_impl = {
	.get_texture_formats = pixman_get_texture_formats,
	.get_render_formats = pixman_get_render_formats,
	.texture_from_buffer = pixman_texture_from_buffer,
	.destroy = pixman_destroy,
	.begin_buffer_pass = pixman_begin_buffer_pass,
};

struct wlr_renderer *wlr_pixman_renderer_create(void) {
	struct wlr_pixman_renderer *renderer = calloc(1, sizeof(*renderer));
	if (renderer == NULL) {
		return NULL;
	}

	wlr_log(WLR_INFO, "Creating pixman renderer");
	wlr_renderer_init(&renderer->wlr_renderer, &renderer_impl, WLR_BUFFER_CAP_DATA_PTR);
	renderer->wlr_renderer.features.output_color_transform = false;
	wl_list_init(&renderer->buffers);
	wl_list_init(&renderer->textures);

	size_t len = 0;
	const uint32_t *formats = get_pixman_drm_formats(&len);

	for (size_t i = 0; i < len; ++i) {
		wlr_drm_format_set_add(&renderer->drm_formats, formats[i],
			DRM_FORMAT_MOD_INVALID);
		wlr_drm_format_set_add(&renderer->drm_formats, formats[i],
			DRM_FORMAT_MOD_LINEAR);
	}

	return &renderer->wlr_renderer;
}

pixman_image_t *wlr_pixman_renderer_get_buffer_image(
		struct wlr_renderer *wlr_renderer, struct wlr_buffer *wlr_buffer) {
	struct wlr_pixman_renderer *renderer = get_renderer(wlr_renderer);
	struct wlr_pixman_buffer *buffer = get_buffer(renderer, wlr_buffer);
	if (!buffer) {
		buffer = create_buffer(renderer, wlr_buffer);
	}
	if (!buffer) {
		return NULL;
	}
	return buffer->image;
}

pixman_image_t *wlr_pixman_texture_get_image(struct wlr_texture *wlr_texture) {
	struct wlr_pixman_texture *texture = get_texture(wlr_texture);
	return texture->image;
}
