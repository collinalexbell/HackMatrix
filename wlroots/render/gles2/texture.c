#include <assert.h>
#include <drm_fourcc.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdint.h>
#include <stdlib.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <wlr/render/egl.h>
#include <wlr/render/interface.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/util/log.h>
#include "render/egl.h"
#include "render/gles2.h"
#include "render/pixel_format.h"

static const struct wlr_texture_impl texture_impl;

bool wlr_texture_is_gles2(struct wlr_texture *wlr_texture) {
	return wlr_texture->impl == &texture_impl;
}

struct wlr_gles2_texture *gles2_get_texture(
		struct wlr_texture *wlr_texture) {
	assert(wlr_texture_is_gles2(wlr_texture));
	struct wlr_gles2_texture *texture = wl_container_of(wlr_texture, texture, wlr_texture);
	return texture;
}

static bool gles2_texture_update_from_buffer(struct wlr_texture *wlr_texture,
		struct wlr_buffer *buffer, const pixman_region32_t *damage) {
	struct wlr_gles2_texture *texture = gles2_get_texture(wlr_texture);

	if (texture->drm_format == DRM_FORMAT_INVALID) {
		return false;
	}

	void *data;
	uint32_t format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
		return false;
	}

	if (format != texture->drm_format) {
		wlr_buffer_end_data_ptr_access(buffer);
		return false;
	}

	const struct wlr_gles2_pixel_format *fmt =
		get_gles2_format_from_drm(texture->drm_format);
	assert(fmt);

	const struct wlr_pixel_format_info *drm_fmt =
		drm_get_pixel_format_info(texture->drm_format);
	assert(drm_fmt);
	if (pixel_format_info_pixels_per_block(drm_fmt) != 1) {
		wlr_buffer_end_data_ptr_access(buffer);
		wlr_log(WLR_ERROR, "Cannot update texture: block formats are not supported");
		return false;
	}

	if (!pixel_format_info_check_stride(drm_fmt, stride, buffer->width)) {
		wlr_buffer_end_data_ptr_access(buffer);
		return false;
	}

	struct wlr_egl_context prev_ctx;
	wlr_egl_make_current(texture->renderer->egl, &prev_ctx);

	push_gles2_debug(texture->renderer);

	glBindTexture(GL_TEXTURE_2D, texture->tex);

	int rects_len = 0;
	const pixman_box32_t *rects = pixman_region32_rectangles(damage, &rects_len);

	for (int i = 0; i < rects_len; i++) {
		pixman_box32_t rect = rects[i];

		glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / drm_fmt->bytes_per_block);
		glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, rect.x1);
		glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, rect.y1);

		int width = rect.x2 - rect.x1;
		int height = rect.y2 - rect.y1;
		glTexSubImage2D(GL_TEXTURE_2D, 0, rect.x1, rect.y1, width, height,
			fmt->gl_format, fmt->gl_type, data);
	}

	glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
	glPixelStorei(GL_UNPACK_SKIP_PIXELS_EXT, 0);
	glPixelStorei(GL_UNPACK_SKIP_ROWS_EXT, 0);

	glBindTexture(GL_TEXTURE_2D, 0);

	pop_gles2_debug(texture->renderer);

	wlr_egl_restore_context(&prev_ctx);

	wlr_buffer_end_data_ptr_access(buffer);

	return true;
}

void gles2_texture_destroy(struct wlr_gles2_texture *texture) {
	wl_list_remove(&texture->link);
	if (texture->buffer != NULL) {
		wlr_buffer_unlock(texture->buffer->buffer);
	} else {
		struct wlr_egl_context prev_ctx;
		wlr_egl_make_current(texture->renderer->egl, &prev_ctx);

		push_gles2_debug(texture->renderer);

		glDeleteTextures(1, &texture->tex);
		glDeleteFramebuffers(1, &texture->fbo);

		pop_gles2_debug(texture->renderer);

		wlr_egl_restore_context(&prev_ctx);
	}

	free(texture);
}

static void handle_gles2_texture_destroy(struct wlr_texture *wlr_texture) {
	gles2_texture_destroy(gles2_get_texture(wlr_texture));
}

static bool gles2_texture_bind(struct wlr_gles2_texture *texture) {
	if (texture->fbo) {
		glBindFramebuffer(GL_FRAMEBUFFER, texture->fbo);
	} else if (texture->buffer) {
		if (texture->buffer->external_only) {
			return false;
		}

		GLuint fbo = gles2_buffer_get_fbo(texture->buffer);
		if (!fbo) {
			return false;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	} else {
		glGenFramebuffers(1, &texture->fbo);
		glBindFramebuffer(GL_FRAMEBUFFER, texture->fbo);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			texture->target, texture->tex, 0);

		GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (fb_status != GL_FRAMEBUFFER_COMPLETE) {
			wlr_log(WLR_ERROR, "Failed to create FBO");
			glDeleteFramebuffers(1, &texture->fbo);
			texture->fbo = 0;

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return false;
		}
	}

	return true;
}

static bool gles2_texture_read_pixels(struct wlr_texture *wlr_texture,
		const struct wlr_texture_read_pixels_options *options) {
	struct wlr_gles2_texture *texture = gles2_get_texture(wlr_texture);

	struct wlr_box src;
	wlr_texture_read_pixels_options_get_src_box(options, wlr_texture, &src);

	const struct wlr_gles2_pixel_format *fmt =
		get_gles2_format_from_drm(options->format);
	if (fmt == NULL || !is_gles2_pixel_format_supported(texture->renderer, fmt)) {
		wlr_log(WLR_ERROR, "Cannot read pixels: unsupported pixel format 0x%"PRIX32, options->format);
		return false;
	}

	if (fmt->gl_format == GL_BGRA_EXT && !texture->renderer->exts.EXT_read_format_bgra) {
		wlr_log(WLR_ERROR,
			"Cannot read pixels: missing GL_EXT_read_format_bgra extension");
		return false;
	}

	const struct wlr_pixel_format_info *drm_fmt =
		drm_get_pixel_format_info(fmt->drm_format);
	assert(drm_fmt);
	if (pixel_format_info_pixels_per_block(drm_fmt) != 1) {
		wlr_log(WLR_ERROR, "Cannot read pixels: block formats are not supported");
		return false;
	}

	push_gles2_debug(texture->renderer);
	struct wlr_egl_context prev_ctx;
	if (!wlr_egl_make_current(texture->renderer->egl, &prev_ctx)) {
		return false;
	}

	if (!gles2_texture_bind(texture)) {
		return false;
	}

	// Make sure any pending drawing is finished before we try to read it
	glFinish();

	glGetError(); // Clear the error flag

	unsigned char *p = wlr_texture_read_pixel_options_get_data(options);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	uint32_t pack_stride = pixel_format_info_min_stride(drm_fmt, src.width);
	if (pack_stride == options->stride && options->dst_x == 0) {
		// Under these particular conditions, we can read the pixels with only
		// one glReadPixels call

		glReadPixels(src.x, src.y, src.width, src.height, fmt->gl_format, fmt->gl_type, p);
	} else {
		// Unfortunately GLES2 doesn't support GL_PACK_ROW_LENGTH, so we have to read
		// the lines out row by row
		for (int32_t i = 0; i < src.height; ++i) {
			uint32_t y = src.y + i;
			glReadPixels(src.x, y, src.width, 1, fmt->gl_format,
				fmt->gl_type, p + i * options->stride);
		}
	}

	wlr_egl_restore_context(&prev_ctx);
	pop_gles2_debug(texture->renderer);

	return glGetError() == GL_NO_ERROR;
}

static uint32_t gles2_texture_preferred_read_format(struct wlr_texture *wlr_texture) {
	struct wlr_gles2_texture *texture = gles2_get_texture(wlr_texture);

	push_gles2_debug(texture->renderer);

	uint32_t fmt = DRM_FORMAT_INVALID;

	struct wlr_egl_context prev_ctx;
	if (!wlr_egl_make_current(texture->renderer->egl, &prev_ctx)) {
		return fmt;
	}

	if (!gles2_texture_bind(texture)) {
		goto out;
	}

	GLint gl_format = -1, gl_type = -1, alpha_size = -1;
	glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &gl_format);
	glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE, &gl_type);
	glGetIntegerv(GL_ALPHA_BITS, &alpha_size);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	pop_gles2_debug(texture->renderer);

	const struct wlr_gles2_pixel_format *pix_fmt =
		get_gles2_format_from_gl(gl_format, gl_type, alpha_size > 0);
	if (pix_fmt != NULL) {
		fmt = pix_fmt->drm_format;
		goto out;
	}

	if (texture->renderer->exts.EXT_read_format_bgra) {
		fmt = DRM_FORMAT_XRGB8888;
		goto out;
	}

out:
	wlr_egl_restore_context(&prev_ctx);
	return fmt;
}

static const struct wlr_texture_impl texture_impl = {
	.update_from_buffer = gles2_texture_update_from_buffer,
	.read_pixels = gles2_texture_read_pixels,
	.preferred_read_format = gles2_texture_preferred_read_format,
	.destroy = handle_gles2_texture_destroy,
};

static struct wlr_gles2_texture *gles2_texture_create(
		struct wlr_gles2_renderer *renderer, uint32_t width, uint32_t height) {
	struct wlr_gles2_texture *texture = calloc(1, sizeof(*texture));
	if (texture == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return NULL;
	}
	wlr_texture_init(&texture->wlr_texture, &renderer->wlr_renderer,
		&texture_impl, width, height);
	texture->renderer = renderer;
	wl_list_insert(&renderer->textures, &texture->link);
	return texture;
}

static struct wlr_texture *gles2_texture_from_pixels(
		struct wlr_renderer *wlr_renderer,
		uint32_t drm_format, uint32_t stride, uint32_t width,
		uint32_t height, const void *data) {
	struct wlr_gles2_renderer *renderer = gles2_get_renderer(wlr_renderer);

	const struct wlr_gles2_pixel_format *fmt =
		get_gles2_format_from_drm(drm_format);
	if (fmt == NULL) {
		wlr_log(WLR_ERROR, "Unsupported pixel format 0x%"PRIX32, drm_format);
		return NULL;
	}

	const struct wlr_pixel_format_info *drm_fmt =
		drm_get_pixel_format_info(drm_format);
	assert(drm_fmt);
	if (pixel_format_info_pixels_per_block(drm_fmt) != 1) {
		wlr_log(WLR_ERROR, "Cannot upload texture: block formats are not supported");
		return NULL;
	}

	if (!pixel_format_info_check_stride(drm_fmt, stride, width)) {
		return NULL;
	}

	struct wlr_gles2_texture *texture =
		gles2_texture_create(renderer, width, height);
	if (texture == NULL) {
		return NULL;
	}
	texture->target = GL_TEXTURE_2D;
	texture->has_alpha = pixel_format_has_alpha(fmt->drm_format);
	texture->drm_format = fmt->drm_format;

	GLint internal_format = fmt->gl_internalformat;
	if (!internal_format) {
		internal_format = fmt->gl_format;
	}

	struct wlr_egl_context prev_ctx;
	wlr_egl_make_current(renderer->egl, &prev_ctx);

	push_gles2_debug(renderer);

	glGenTextures(1, &texture->tex);
	glBindTexture(GL_TEXTURE_2D, texture->tex);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, stride / drm_fmt->bytes_per_block);
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0,
		fmt->gl_format, fmt->gl_type, data);
	glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);

	glBindTexture(GL_TEXTURE_2D, 0);

	pop_gles2_debug(renderer);

	wlr_egl_restore_context(&prev_ctx);

	return &texture->wlr_texture;
}

static struct wlr_texture *gles2_texture_from_dmabuf(
		struct wlr_gles2_renderer *renderer, struct wlr_buffer *wlr_buffer,
		struct wlr_dmabuf_attributes *attribs) {
	if (!renderer->procs.glEGLImageTargetTexture2DOES) {
		return NULL;
	}

	struct wlr_gles2_buffer *buffer = gles2_buffer_get_or_create(renderer, wlr_buffer);
	if (!buffer) {
		return NULL;
	}

	struct wlr_gles2_texture *texture =
		gles2_texture_create(renderer, attribs->width, attribs->height);
	if (texture == NULL) {
		return NULL;
	}

	texture->target = buffer->external_only ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
	texture->buffer = buffer;
	texture->drm_format = DRM_FORMAT_INVALID; // texture can't be written anyways
	texture->has_alpha = pixel_format_has_alpha(attribs->format);

	struct wlr_egl_context prev_ctx;
	wlr_egl_make_current(renderer->egl, &prev_ctx);
	push_gles2_debug(texture->renderer);

	bool invalid;
	if (!buffer->tex) {
		glGenTextures(1, &buffer->tex);
		invalid = true;
	} else {
		// External changes are immediately made visible by the GL implementation
		invalid = !buffer->external_only;
	}

	if (invalid) {
		glBindTexture(texture->target, buffer->tex);
		glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		renderer->procs.glEGLImageTargetTexture2DOES(texture->target, buffer->image);
		glBindTexture(texture->target, 0);
	}

	pop_gles2_debug(texture->renderer);
	wlr_egl_restore_context(&prev_ctx);

	texture->tex = buffer->tex;
	wlr_buffer_lock(texture->buffer->buffer);
	return &texture->wlr_texture;
}

struct wlr_texture *gles2_texture_from_buffer(struct wlr_renderer *wlr_renderer,
		struct wlr_buffer *buffer) {
	struct wlr_gles2_renderer *renderer = gles2_get_renderer(wlr_renderer);

	void *data;
	uint32_t format;
	size_t stride;
	struct wlr_dmabuf_attributes dmabuf;
	if (wlr_buffer_get_dmabuf(buffer, &dmabuf)) {
		return gles2_texture_from_dmabuf(renderer, buffer, &dmabuf);
	} else if (wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
		struct wlr_texture *tex = gles2_texture_from_pixels(wlr_renderer,
			format, stride, buffer->width, buffer->height, data);
		wlr_buffer_end_data_ptr_access(buffer);
		return tex;
	} else {
		return NULL;
	}
}

void wlr_gles2_texture_get_attribs(struct wlr_texture *wlr_texture,
		struct wlr_gles2_texture_attribs *attribs) {
	struct wlr_gles2_texture *texture = gles2_get_texture(wlr_texture);
	*attribs = (struct wlr_gles2_texture_attribs){
		.target = texture->target,
		.tex = texture->tex,
		.has_alpha = texture->has_alpha,
	};
}
