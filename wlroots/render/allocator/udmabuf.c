#undef _POSIX_C_SOURCE
#define _GNU_SOURCE // for memfd_create() and F_ADD_SEALS

#include <drm_fourcc.h>
#include <fcntl.h>
#include <linux/udmabuf.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/allocator.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/util/log.h>

#include "render/allocator/udmabuf.h"
#include "render/pixel_format.h"

static bool buffer_get_shm(struct wlr_buffer *wlr_buffer, struct wlr_shm_attributes *shm) {
	struct wlr_udmabuf_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	*shm = buffer->shm;
	return true;
}

static bool buffer_get_dmabuf(struct wlr_buffer *wlr_buffer, struct wlr_dmabuf_attributes *dmabuf) {
	struct wlr_udmabuf_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	*dmabuf = buffer->dmabuf;
	return true;
}

static void buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct wlr_udmabuf_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	wlr_dmabuf_attributes_finish(&buffer->dmabuf);
	close(buffer->shm.fd);
	free(buffer);
}

static const struct wlr_buffer_impl buffer_impl = {
	.destroy = buffer_destroy,
	.get_shm = buffer_get_shm,
	.get_dmabuf = buffer_get_dmabuf,
};

static struct wlr_buffer *allocator_create_buffer(
		struct wlr_allocator *wlr_allocator, int width, int height,
		const struct wlr_drm_format *format) {
	struct wlr_udmabuf_allocator *allocator = wl_container_of(wlr_allocator, allocator, base);

	const struct wlr_pixel_format_info *info =
		drm_get_pixel_format_info(format->format);
	if (info == NULL) {
		wlr_log(WLR_ERROR, "Unsupported pixel format 0x%"PRIX32, format->format);
		return NULL;
	}

	long page_size = sysconf(_SC_PAGE_SIZE);
	if (page_size == -1) {
		wlr_log_errno(WLR_ERROR, "Failed to query page size");
		return NULL;
	}

	struct wlr_udmabuf_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return NULL;
	}
	wlr_buffer_init(&buffer->base, &buffer_impl, width, height);

	// TODO: consider using a single file for multiple buffers
	int stride = pixel_format_info_min_stride(info, width); // TODO: align?
	size_t size = stride * height;
	if (size % page_size != 0) {
		size += page_size - (size % page_size);
	}

	int memfd = memfd_create("wlroots", MFD_CLOEXEC | MFD_ALLOW_SEALING);
	if (memfd < 0) {
		wlr_log_errno(WLR_ERROR, "memfd_create() failed");
		goto err_buffer;
	}

	if (ftruncate(memfd, size) < 0) {
		wlr_log_errno(WLR_ERROR, "ftruncate() failed");
		goto err_memfd;
	}

	if (fcntl(memfd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK) < 0) {
		wlr_log_errno(WLR_ERROR, "fcntl(F_ADD_SEALS) failed");
		goto err_memfd;
	}

	struct udmabuf_create udmabuf_create = {
		.memfd = memfd,
		.flags = UDMABUF_FLAGS_CLOEXEC,
		.offset = 0,
		.size = size,
	};
	int dmabuf_fd = ioctl(allocator->fd, UDMABUF_CREATE, &udmabuf_create);
	if (dmabuf_fd < 0) {
		wlr_log_errno(WLR_ERROR, "ioctl(UDMABUF_CREATE) failed");
		goto err_memfd;
	}

	buffer->size = size;
	buffer->shm = (struct wlr_shm_attributes){
		.width = width,
		.height = height,
		.format = format->format,
		.offset = 0,
		.stride = stride,
		.fd = memfd,
	};
	buffer->dmabuf = (struct wlr_dmabuf_attributes){
		.width = width,
		.height = height,
		.format = format->format,
		.modifier = DRM_FORMAT_MOD_LINEAR,
		.n_planes = 1,
		.offset[0] = 0,
		.stride[0] = stride,
		.fd[0] = dmabuf_fd,
	};

	return &buffer->base;

err_memfd:
	close(memfd);
err_buffer:
	free(buffer);
	return NULL;
}

static void allocator_destroy(struct wlr_allocator *wlr_allocator) {
	struct wlr_udmabuf_allocator *allocator = wl_container_of(wlr_allocator, allocator, base);
	close(allocator->fd);
	free(allocator);
}

static const struct wlr_allocator_interface allocator_impl = {
	.destroy = allocator_destroy,
	.create_buffer = allocator_create_buffer,
};

struct wlr_allocator *wlr_udmabuf_allocator_create(void) {
	int fd = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
	if (fd < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to open /dev/udmabuf");
		return NULL;
	}

	struct wlr_udmabuf_allocator *allocator = calloc(1, sizeof(*allocator));
	if (allocator == NULL) {
		close(fd);
		return NULL;
	}
	wlr_allocator_init(&allocator->base, &allocator_impl,
		WLR_BUFFER_CAP_SHM | WLR_BUFFER_CAP_DMABUF);

	allocator->fd = fd;

	return &allocator->base;
}
