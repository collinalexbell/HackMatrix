#undef _POSIX_C_SOURCE
#define _DEFAULT_SOURCE // for MAP_ANONYMOUS
#include <assert.h>
#include <drm_fourcc.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-server-protocol.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_shm.h>
#include <wlr/util/log.h>
#include "render/pixel_format.h"

#ifdef __STDC_NO_ATOMICS__
#error "C11 atomics are required"
#endif
#if ATOMIC_POINTER_LOCK_FREE == 0
#error "Lock-free C11 atomic pointers are required"
#endif

#define SHM_VERSION 2

struct wlr_shm_pool {
	struct wl_resource *resource; // may be NULL
	struct wlr_shm *shm;
	struct wl_list buffers; // wlr_shm_buffer.link
	int fd;
	struct wlr_shm_mapping *mapping;
};

/**
 * A mapped space to a client-owned file.
 *
 * Clients may resize pools via wl_shm_pool.resize, in which case we need to
 * re-map the FD with a larger size. However we might be at the same time still
 * accessing the old mapping (via wlr_buffer_begin_data_ptr_access()). We need
 * to keep the old mapping alive in that case.
 */
struct wlr_shm_mapping {
	void *data;
	size_t size;
	bool dropped; // false while a wlr_shm_pool references this mapping
};

struct wlr_shm_sigbus_data {
	struct wlr_shm_mapping *mapping;
	struct sigaction prev_action;
	struct wlr_shm_sigbus_data *_Atomic next;
};

struct wlr_shm_buffer {
	struct wlr_buffer base;
	struct wlr_shm_pool *pool;
	uint32_t drm_format;
	int32_t stride;
	off_t offset;
	struct wl_list link; // wlr_shm_pool.buffers
	struct wl_resource *resource; // may be NULL

	struct wl_listener release;

	struct wlr_shm_sigbus_data sigbus_data;
};

// Needs to be a lock-free atomic because it's accessed from a signal handler
static struct wlr_shm_sigbus_data *_Atomic sigbus_data = NULL;

static const struct wl_buffer_interface wl_buffer_impl;
static const struct wl_shm_pool_interface pool_impl;
static const struct wl_shm_interface shm_impl;

static bool buffer_resource_is_instance(struct wl_resource *resource) {
	return wl_resource_instance_of(resource, &wl_buffer_interface,
		&wl_buffer_impl);
}

static struct wlr_shm_buffer *buffer_from_resource(struct wl_resource *resource) {
	assert(buffer_resource_is_instance(resource));
	return wl_resource_get_user_data(resource);
}

static struct wlr_buffer *buffer_base_from_resource(struct wl_resource *resource) {
	return &buffer_from_resource(resource)->base;
}

static struct wlr_shm_pool *pool_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wl_shm_pool_interface,
		&pool_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_shm *shm_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wl_shm_interface, &shm_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_shm_mapping *mapping_create(int fd, size_t size) {
	void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		wlr_log_errno(WLR_DEBUG, "mmap failed");
		return NULL;
	}

	struct wlr_shm_mapping *mapping = calloc(1, sizeof(*mapping));
	if (mapping == NULL) {
		munmap(data, size);
		return NULL;
	}

	mapping->data = data;
	mapping->size = size;
	return mapping;
}

static void mapping_consider_destroy(struct wlr_shm_mapping *mapping) {
	if (!mapping->dropped) {
		return;
	}

	for (struct wlr_shm_sigbus_data *cur = sigbus_data; cur != NULL; cur = cur->next) {
		if (cur->mapping == mapping) {
			return;
		}
	}

	munmap(mapping->data, mapping->size);
	free(mapping);
}

/**
 * Indicate that this mapping is no longer used by its wlr_shm_pool owner.
 *
 * May destroy the mapping.
 */
static void mapping_drop(struct wlr_shm_mapping *mapping) {
	if (mapping == NULL) {
		return;
	}

	mapping->dropped = true;
	mapping_consider_destroy(mapping);
}

static const struct wlr_buffer_resource_interface buffer_resource_interface = {
	.name = "wl_shm",
	.is_instance = buffer_resource_is_instance,
	.from_resource = buffer_base_from_resource,
};

static void pool_consider_destroy(struct wlr_shm_pool *pool);

static void buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct wlr_shm_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	wl_list_remove(&buffer->release.link);

	wlr_buffer_finish(wlr_buffer);

	assert(buffer->resource == NULL);
	wl_list_remove(&buffer->link);
	pool_consider_destroy(buffer->pool);
	free(buffer);
}

static bool buffer_get_shm(struct wlr_buffer *wlr_buffer,
		struct wlr_shm_attributes *attrs) {
	struct wlr_shm_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	*attrs = (struct wlr_shm_attributes){
		.fd = buffer->pool->fd,
		.format = buffer->drm_format,
		.width = buffer->base.width,
		.height = buffer->base.height,
		.stride = buffer->stride,
		.offset = buffer->offset,
	};
	return true;
}

static void handle_sigbus(int sig, siginfo_t *info, void *context) {
	assert(sigbus_data != NULL);
	struct sigaction prev_action = sigbus_data->prev_action;

	// Check whether the offending address is inside of the wl_shm_pool's mapped
	// space
	uintptr_t addr = (uintptr_t)info->si_addr;
	struct wlr_shm_mapping *mapping = NULL;
	for (struct wlr_shm_sigbus_data *data = sigbus_data; data != NULL; data = data->next) {
		uintptr_t mapping_start = (uintptr_t)data->mapping->data;
		size_t mapping_size = data->mapping->size;
		if (addr >= mapping_start && addr < mapping_start + mapping_size) {
			mapping = data->mapping;
			break;
		}
	}
	if (mapping == NULL) {
		goto reraise;
	}

	// Replace the mapping with a new one which won't cause SIGBUS (instead, it
	// will read as zeroes). Technically mmap() isn't part of the
	// async-signal-safe functions...
	if (mmap(mapping->data, mapping->size, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0) == MAP_FAILED) {
		goto reraise;
	}

	return;

reraise:
	if (prev_action.sa_flags & SA_SIGINFO) {
		prev_action.sa_sigaction(sig, info, context);
	} else {
		prev_action.sa_handler(sig);
	}
}

static bool buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct wlr_shm_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);

	if (!atomic_is_lock_free(&sigbus_data)) {
		wlr_log(WLR_ERROR, "Lock-free atomic pointers are required");
		return false;
	}

	// Install a SIGBUS handler. SIGBUS is triggered if the client shrinks the
	// backing file, and then we try to access the mapping.
	struct sigaction prev_action;
	if (sigbus_data == NULL) {
		struct sigaction new_action = {
			.sa_sigaction = handle_sigbus,
			.sa_flags = SA_SIGINFO | SA_NODEFER,
		};
		if (sigaction(SIGBUS, &new_action, &prev_action) != 0) {
			wlr_log_errno(WLR_ERROR, "sigaction failed");
			return false;
		}
	} else {
		prev_action = sigbus_data->prev_action;
	}

	struct wlr_shm_mapping *mapping = buffer->pool->mapping;

	buffer->sigbus_data = (struct wlr_shm_sigbus_data){
		.mapping = mapping,
		.prev_action = prev_action,
		.next = sigbus_data,
	};
	sigbus_data = &buffer->sigbus_data;

	*data = (char *)mapping->data + buffer->offset;
	*format = buffer->drm_format;
	*stride = buffer->stride;
	return true;
}

static void buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	struct wlr_shm_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);

	if (sigbus_data == &buffer->sigbus_data) {
		sigbus_data = buffer->sigbus_data.next;
	} else {
		for (struct wlr_shm_sigbus_data *cur = sigbus_data; cur != NULL; cur = cur->next) {
			if (cur->next == &buffer->sigbus_data) {
				cur->next = buffer->sigbus_data.next;
				break;
			}
		}
	}

	if (sigbus_data == NULL) {
		if (sigaction(SIGBUS, &buffer->sigbus_data.prev_action, NULL) != 0) {
			wlr_log_errno(WLR_ERROR, "sigaction failed");
		}
	}

	mapping_consider_destroy(buffer->sigbus_data.mapping);
}

static const struct wlr_buffer_impl buffer_impl = {
	.destroy = buffer_destroy,
	.get_shm = buffer_get_shm,
	.begin_data_ptr_access = buffer_begin_data_ptr_access,
	.end_data_ptr_access = buffer_end_data_ptr_access,
};

static void destroy_resource(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wl_buffer_interface wl_buffer_impl = {
	.destroy = destroy_resource,
};

static void buffer_handle_release(struct wl_listener *listener, void *data) {
	struct wlr_shm_buffer *buffer = wl_container_of(listener, buffer, release);
	if (buffer->resource != NULL) {
		wl_buffer_send_release(buffer->resource);
	}
}

static void buffer_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_shm_buffer *buffer = buffer_from_resource(resource);
	buffer->resource = NULL;
	wlr_buffer_drop(&buffer->base);
}

static bool shm_has_format(struct wlr_shm *shm, uint32_t shm_format);

static void pool_handle_create_buffer(struct wl_client *client,
		struct wl_resource *pool_resource, uint32_t id, int32_t offset,
		int32_t width, int32_t height, int32_t stride, uint32_t shm_format) {
	struct wlr_shm_pool *pool = pool_from_resource(pool_resource);

	// Convert to uint64_t to avoid integer overflow
	if (offset < 0 || width <= 0 || height <= 0 || stride < width ||
			offset + (uint64_t)stride * height > pool->mapping->size) {
		wl_resource_post_error(pool_resource, WL_SHM_ERROR_INVALID_STRIDE,
			"Invalid width, height or stride (%dx%d, %d)",
			width, height, stride);
		return;
	}

	if (!shm_has_format(pool->shm, shm_format)) {
		wl_resource_post_error(pool_resource, WL_SHM_ERROR_INVALID_FORMAT,
			"Unsupported format");
		return;
	}

	uint32_t drm_format = convert_wl_shm_format_to_drm(shm_format);
	const struct wlr_pixel_format_info *format_info =
		drm_get_pixel_format_info(drm_format);
	if (format_info == NULL) {
		wl_resource_post_error(pool_resource, WL_SHM_ERROR_INVALID_FORMAT,
			"Unknown format");
		return;
	}
	if (!pixel_format_info_check_stride(format_info, stride, width)) {
		wl_resource_post_error(pool_resource, WL_SHM_ERROR_INVALID_STRIDE,
			"Invalid stride (%d)", stride);
		return;
	}

	struct wlr_shm_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		wl_resource_post_no_memory(pool_resource);
		return;
	}

	buffer->resource = wl_resource_create(client, &wl_buffer_interface, 1, id);
	if (buffer->resource == NULL) {
		free(buffer);
		wl_resource_post_no_memory(pool_resource);
		return;
	}

	buffer->pool = pool;
	buffer->offset = offset;
	buffer->stride = stride;
	buffer->drm_format = drm_format;
	wlr_buffer_init(&buffer->base, &buffer_impl, width, height);
	wl_resource_set_implementation(buffer->resource,
		&wl_buffer_impl, buffer, buffer_handle_resource_destroy);

	wl_list_insert(&pool->buffers, &buffer->link);

	buffer->release.notify = buffer_handle_release;
	wl_signal_add(&buffer->base.events.release, &buffer->release);
}

static void pool_handle_resize(struct wl_client *client,
		struct wl_resource *pool_resource, int32_t size) {
	struct wlr_shm_pool *pool = pool_from_resource(pool_resource);

	if (size <= 0 || (size_t)size < pool->mapping->size) {
		wl_resource_post_error(pool_resource, WL_SHM_ERROR_INVALID_STRIDE,
			"Shrinking a pool (%zu to %d) is forbidden",
			pool->mapping->size, size);
		return;
	}

	struct wlr_shm_mapping *mapping = mapping_create(pool->fd, size);
	if (mapping == NULL) {
		wl_resource_post_error(pool_resource, WL_SHM_ERROR_INVALID_FD,
			"Failed to create memory mapping");
		return;
	}

	mapping_drop(pool->mapping);
	pool->mapping = mapping;
}

static const struct wl_shm_pool_interface pool_impl = {
	.create_buffer = pool_handle_create_buffer,
	.destroy = destroy_resource,
	.resize = pool_handle_resize,
};

static void pool_consider_destroy(struct wlr_shm_pool *pool) {
	if (pool->resource != NULL || !wl_list_empty(&pool->buffers)) {
		return;
	}

	mapping_drop(pool->mapping);
	close(pool->fd);
	free(pool);
}

static void pool_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_shm_pool *pool = pool_from_resource(resource);
	pool->resource = NULL;
	pool_consider_destroy(pool);
}

static void shm_handle_create_pool(struct wl_client *client,
		struct wl_resource *shm_resource, uint32_t id, int fd, int32_t size) {
	struct wlr_shm *shm = shm_from_resource(shm_resource);

	if (size <= 0) {
		wl_resource_post_error(shm_resource, WL_SHM_ERROR_INVALID_STRIDE,
			"Invalid size (%d)", size);
		goto error_fd;
	}

	struct wlr_shm_mapping *mapping = mapping_create(fd, size);
	if (mapping == NULL) {
		wl_resource_post_error(shm_resource, WL_SHM_ERROR_INVALID_FD,
			"Failed to create memory mapping");
		goto error_fd;
	}

	struct wlr_shm_pool *pool = calloc(1, sizeof(*pool));
	if (pool == NULL) {
		wl_resource_post_no_memory(shm_resource);
		goto error_mapping;
	}

	uint32_t version = wl_resource_get_version(shm_resource);
	pool->resource =
		wl_resource_create(client, &wl_shm_pool_interface, version, id);
	if (pool->resource == NULL) {
		wl_resource_post_no_memory(shm_resource);
		goto error_pool;
	}
	wl_resource_set_implementation(pool->resource, &pool_impl, pool,
		pool_handle_resource_destroy);

	pool->mapping = mapping;
	pool->shm = shm;
	pool->fd = fd;
	wl_list_init(&pool->buffers);
	return;

error_pool:
	free(pool);
error_mapping:
	mapping_drop(mapping);
error_fd:
	close(fd);
}

static void shm_handle_release(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wl_shm_interface shm_impl = {
	.create_pool = shm_handle_create_pool,
	.release = shm_handle_release,
};

static void shm_bind(struct wl_client *client, void *data, uint32_t version,
		uint32_t id) {
	struct wlr_shm *shm = data;

	struct wl_resource *resource = wl_resource_create(client, &wl_shm_interface,
		version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &shm_impl, shm, NULL);

	for (size_t i = 0; i < shm->formats_len; i++) {
		wl_shm_send_format(resource, shm->formats[i]);
	}
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_shm *shm = wl_container_of(listener, shm, display_destroy);
	wl_list_remove(&shm->display_destroy.link);
	wl_global_destroy(shm->global);
	free(shm->formats);
	free(shm);
}

struct wlr_shm *wlr_shm_create(struct wl_display *display, uint32_t version,
		const uint32_t *formats, size_t formats_len) {
	assert(version <= SHM_VERSION);

	// ARGB8888 and XRGB8888 must be supported per the wl_shm spec
	bool has_argb8888 = false, has_xrgb8888 = false;
	for (size_t i = 0; i < formats_len; i++) {
		switch (formats[i]) {
		case DRM_FORMAT_ARGB8888:
			has_argb8888 = true;
			break;
		case DRM_FORMAT_XRGB8888:
			has_xrgb8888 = true;
			break;
		}
	}
	assert(has_argb8888 && has_xrgb8888);

	struct wlr_shm *shm = calloc(1, sizeof(*shm));
	if (shm == NULL) {
		wlr_log(WLR_ERROR, "Allocation failed");
		return NULL;
	}

	shm->formats_len = formats_len;
	shm->formats = malloc(formats_len * sizeof(uint32_t));
	if (shm->formats == NULL) {
		wlr_log(WLR_ERROR, "Allocation failed");
		free(shm);
		return NULL;
	}
	for (size_t i = 0; i < formats_len; i++) {
		shm->formats[i] = convert_drm_format_to_wl_shm(formats[i]);
	}

	shm->global = wl_global_create(display, &wl_shm_interface, version,
		shm, shm_bind);
	if (shm->global == NULL) {
		wlr_log(WLR_ERROR, "wl_global_create failed");
		free(shm->formats);
		free(shm);
		return NULL;
	}

	shm->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &shm->display_destroy);

	wlr_buffer_register_resource_interface(&buffer_resource_interface);

	return shm;
}

struct wlr_shm *wlr_shm_create_with_renderer(struct wl_display *display,
		uint32_t version, struct wlr_renderer *renderer) {
	const struct wlr_drm_format_set *format_set =
		wlr_renderer_get_texture_formats(renderer, WLR_BUFFER_CAP_DATA_PTR);
	if (format_set == NULL || format_set->len == 0) {
		wlr_log(WLR_ERROR, "Failed to initialize wl_shm: "
			"cannot get renderer formats");
		return NULL;
	}

	size_t formats_len = format_set->len;
	uint32_t *formats = calloc(formats_len, sizeof(formats[0]));
	if (formats == NULL) {
		return NULL;
	}

	for (size_t i = 0; i < format_set->len; i++) {
		formats[i] = format_set->formats[i].format;
	}

	struct wlr_shm *shm = wlr_shm_create(display, version, formats, formats_len);
	free(formats);
	return shm;
}

static bool shm_has_format(struct wlr_shm *shm, uint32_t shm_format) {
	for (size_t i = 0; i < shm->formats_len; i++) {
		if (shm->formats[i] == shm_format) {
			return true;
		}
	}
	return false;
}
