#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/util/log.h>
#include "types/wlr_buffer.h"

static const struct wlr_buffer_impl readonly_data_buffer_impl;

static struct wlr_readonly_data_buffer *readonly_data_buffer_from_buffer(
		struct wlr_buffer *wlr_buffer) {
	assert(wlr_buffer->impl == &readonly_data_buffer_impl);
	struct wlr_readonly_data_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	return buffer;
}

static void readonly_data_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct wlr_readonly_data_buffer *buffer =
		readonly_data_buffer_from_buffer(wlr_buffer);
	wlr_buffer_finish(wlr_buffer);
	free(buffer->saved_data);
	free(buffer);
}

static bool readonly_data_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct wlr_readonly_data_buffer *buffer =
		readonly_data_buffer_from_buffer(wlr_buffer);
	if (buffer->data == NULL) {
		return false;
	}
	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}
	*data = (void *)buffer->data;
	*format = buffer->format;
	*stride = buffer->stride;
	return true;
}

static void readonly_data_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
	// This space is intentionally left blank
}

static const struct wlr_buffer_impl readonly_data_buffer_impl = {
	.destroy = readonly_data_buffer_destroy,
	.begin_data_ptr_access = readonly_data_buffer_begin_data_ptr_access,
	.end_data_ptr_access = readonly_data_buffer_end_data_ptr_access,
};

struct wlr_readonly_data_buffer *readonly_data_buffer_create(uint32_t format,
		size_t stride, uint32_t width, uint32_t height, const void *data) {
	struct wlr_readonly_data_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return NULL;
	}
	wlr_buffer_init(&buffer->base, &readonly_data_buffer_impl, width, height);

	buffer->data = data;
	buffer->format = format;
	buffer->stride = stride;

	return buffer;
}

bool readonly_data_buffer_drop(struct wlr_readonly_data_buffer *buffer) {
	bool ok = true;

	if (buffer->base.n_locks > 0) {
		size_t size = buffer->stride * buffer->base.height;
		buffer->saved_data = malloc(size);
		if (buffer->saved_data == NULL) {
			wlr_log_errno(WLR_ERROR, "Allocation failed");
			ok = false;
			buffer->data = NULL;
			// We can't destroy the buffer, or we risk use-after-free in the
			// consumers. We can't allow accesses to buffer->data anymore, so
			// set it to NULL and make subsequent begin_data_ptr_access()
			// calls fail.
		} else {
			memcpy(buffer->saved_data, buffer->data, size);
			buffer->data = buffer->saved_data;
		}
	}

	wlr_buffer_drop(&buffer->base);
	return ok;
}

