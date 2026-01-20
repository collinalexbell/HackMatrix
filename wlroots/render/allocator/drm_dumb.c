
#include <assert.h>
#include <drm_fourcc.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wlr/backend.h>
#include <wlr/backend/session.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/util/log.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "render/allocator/drm_dumb.h"
#include "render/drm_format_set.h"
#include "render/pixel_format.h"

static const struct wlr_buffer_impl buffer_impl;

static struct wlr_drm_dumb_buffer *drm_dumb_buffer_from_buffer(
		struct wlr_buffer *wlr_buf) {
	assert(wlr_buf->impl == &buffer_impl);
	struct wlr_drm_dumb_buffer *buf = wl_container_of(wlr_buf, buf, base);
	return buf;
}

static struct wlr_drm_dumb_buffer *create_buffer(
		struct wlr_drm_dumb_allocator *alloc, int width, int height,
		const struct wlr_drm_format *format) {
	if (!wlr_drm_format_has(format, DRM_FORMAT_MOD_INVALID) &&
			!wlr_drm_format_has(format, DRM_FORMAT_MOD_LINEAR)) {
		wlr_log(WLR_ERROR, "DRM dumb allocator only supports INVALID and "
			"LINEAR modifiers");
		return NULL;
	}

	const struct wlr_pixel_format_info *info =
		drm_get_pixel_format_info(format->format);
	if (info == NULL) {
		wlr_log(WLR_ERROR, "DRM format 0x%"PRIX32" not supported",
			format->format);
		return NULL;
	} else if (pixel_format_info_pixels_per_block(info) != 1) {
		wlr_log(WLR_ERROR, "Block formats are not supported");
		return NULL;
	}

	struct wlr_drm_dumb_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return NULL;
	}
	wlr_buffer_init(&buffer->base, &buffer_impl, width, height);
	wl_list_insert(&alloc->buffers, &buffer->link);

	buffer->drm_fd = alloc->drm_fd;

	uint32_t bpp = 8 * info->bytes_per_block;
	if (drmModeCreateDumbBuffer(alloc->drm_fd, width, height, bpp, 0,
			&buffer->handle, &buffer->stride, &buffer->size) != 0) {
		wlr_log_errno(WLR_ERROR, "Failed to create DRM dumb buffer");
		goto create_destroy;
	}

	buffer->width = width;
	buffer->height = height;
	buffer->format = format->format;

	uint64_t offset;
	if (drmModeMapDumbBuffer(alloc->drm_fd, buffer->handle, &offset) != 0) {
		wlr_log_errno(WLR_ERROR, "Failed to map DRM dumb buffer");
		goto create_destroy;
	}

	buffer->data = mmap(NULL, buffer->size, PROT_READ | PROT_WRITE, MAP_SHARED,
		alloc->drm_fd, offset);
	if (buffer->data == MAP_FAILED) {
		wlr_log_errno(WLR_ERROR, "Failed to mmap DRM dumb buffer");
		goto create_destroy;
	}

	memset(buffer->data, 0, buffer->size);

	int prime_fd;
	if (drmPrimeHandleToFD(alloc->drm_fd, buffer->handle, DRM_CLOEXEC,
			&prime_fd) != 0) {
		wlr_log_errno(WLR_ERROR, "Failed to get PRIME handle from GEM handle");
		goto create_destroy;
	}

	buffer->dmabuf = (struct wlr_dmabuf_attributes){
		.width = buffer->width,
		.height = buffer->height,
		.format = format->format,
		.modifier = DRM_FORMAT_MOD_LINEAR,
		.n_planes = 1,
		.offset[0] = 0,
		.stride[0] = buffer->stride,
		.fd[0] = prime_fd,
	};

	wlr_log(WLR_DEBUG, "Allocated %"PRIu32"x%"PRIu32" DRM dumb buffer",
			buffer->width, buffer->height);

	return buffer;

create_destroy:
	wlr_buffer_drop(&buffer->base);
	return NULL;
}

static bool drm_dumb_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct wlr_drm_dumb_buffer *buf = drm_dumb_buffer_from_buffer(wlr_buffer);
	*data = buf->data;
	*stride = buf->stride;
	*format = buf->format;
	return true;
}

static void drm_dumb_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	// This space is intentionally left blank
}

static bool buffer_get_dmabuf(struct wlr_buffer *wlr_buffer,
		struct wlr_dmabuf_attributes *attribs) {
	struct wlr_drm_dumb_buffer *buf = drm_dumb_buffer_from_buffer(wlr_buffer);
	*attribs = buf->dmabuf;
	return true;
}

static void buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct wlr_drm_dumb_buffer *buf = drm_dumb_buffer_from_buffer(wlr_buffer);

	wlr_buffer_finish(wlr_buffer);

	if (buf->data) {
		munmap(buf->data, buf->size);
	}

	wlr_dmabuf_attributes_finish(&buf->dmabuf);

	if (buf->drm_fd >= 0) {
		if (drmModeDestroyDumbBuffer(buf->drm_fd, buf->handle) != 0) {
			wlr_log_errno(WLR_ERROR, "Failed to destroy DRM dumb buffer");
		}
	}

	wl_list_remove(&buf->link);
	free(buf);
}

static const struct wlr_buffer_impl buffer_impl = {
	.destroy = buffer_destroy,
	.get_dmabuf = buffer_get_dmabuf,
	.begin_data_ptr_access = drm_dumb_buffer_begin_data_ptr_access,
	.end_data_ptr_access = drm_dumb_buffer_end_data_ptr_access,
};

static const struct wlr_allocator_interface allocator_impl;

static struct wlr_drm_dumb_allocator *drm_dumb_alloc_from_alloc(
		struct wlr_allocator *wlr_alloc) {
	assert(wlr_alloc->impl == &allocator_impl);
	struct wlr_drm_dumb_allocator *alloc = wl_container_of(wlr_alloc, alloc, base);
	return alloc;
}

static struct wlr_buffer *allocator_create_buffer(
		struct wlr_allocator *wlr_alloc, int width, int height,
		const struct wlr_drm_format *drm_format) {
	struct wlr_drm_dumb_allocator *alloc = drm_dumb_alloc_from_alloc(wlr_alloc);
	struct wlr_drm_dumb_buffer *buffer = create_buffer(alloc, width, height,
			drm_format);
	if (buffer == NULL) {
		return NULL;
	}
	return &buffer->base;
}

static void allocator_destroy(struct wlr_allocator *wlr_alloc) {
	struct wlr_drm_dumb_allocator *alloc = drm_dumb_alloc_from_alloc(wlr_alloc);

	struct wlr_drm_dumb_buffer *buf, *buf_tmp;
	wl_list_for_each_safe(buf, buf_tmp, &alloc->buffers, link) {
		buf->drm_fd = -1;
		wl_list_remove(&buf->link);
		wl_list_init(&buf->link);
	}

	close(alloc->drm_fd);
	free(alloc);
}

static const struct wlr_allocator_interface allocator_impl = {
	.create_buffer = allocator_create_buffer,
	.destroy = allocator_destroy,
};

struct wlr_allocator *wlr_drm_dumb_allocator_create(int drm_fd) {
	if (drmGetNodeTypeFromFd(drm_fd) != DRM_NODE_PRIMARY) {
		wlr_log(WLR_ERROR, "Cannot use DRM dumb buffers with non-primary DRM FD");
		return NULL;
	}

	uint64_t has_dumb = 0;
	if (drmGetCap(drm_fd, DRM_CAP_DUMB_BUFFER, &has_dumb) < 0) {
		wlr_log(WLR_ERROR, "Failed to get DRM capabilities");
		return NULL;
	}

	if (has_dumb == 0) {
		wlr_log(WLR_ERROR, "DRM dumb buffers not supported");
		return NULL;
	}

	struct wlr_drm_dumb_allocator *alloc = calloc(1, sizeof(*alloc));
	if (alloc == NULL) {
		return NULL;
	}
	wlr_allocator_init(&alloc->base, &allocator_impl,
		WLR_BUFFER_CAP_DATA_PTR | WLR_BUFFER_CAP_DMABUF);

	alloc->drm_fd = drm_fd;
	wl_list_init(&alloc->buffers);

	wlr_log(WLR_DEBUG, "Created DRM dumb allocator");
	return &alloc->base;
}
