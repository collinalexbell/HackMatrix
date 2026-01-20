#include <assert.h>
#include <drm_fourcc.h>
#include <string.h>
#include <wlr/interfaces/wlr_buffer.h>
#include "render/pixel_format.h"
#include "types/wlr_buffer.h"

void wlr_buffer_init(struct wlr_buffer *buffer,
		const struct wlr_buffer_impl *impl, int width, int height) {
	assert(impl->destroy);
	if (impl->begin_data_ptr_access || impl->end_data_ptr_access) {
		assert(impl->begin_data_ptr_access && impl->end_data_ptr_access);
	}

	*buffer = (struct wlr_buffer){
		.impl = impl,
		.width = width,
		.height = height,
	};

	wl_signal_init(&buffer->events.destroy);
	wl_signal_init(&buffer->events.release);

	wlr_addon_set_init(&buffer->addons);
}

void wlr_buffer_finish(struct wlr_buffer *buffer) {
	wl_signal_emit_mutable(&buffer->events.destroy, NULL);
	wlr_addon_set_finish(&buffer->addons);

	assert(wl_list_empty(&buffer->events.destroy.listener_list));
	assert(wl_list_empty(&buffer->events.release.listener_list));
}

static void buffer_consider_destroy(struct wlr_buffer *buffer) {
	if (!buffer->dropped || buffer->n_locks > 0) {
		return;
	}

	assert(!buffer->accessing_data_ptr);

	buffer->impl->destroy(buffer);
}

void wlr_buffer_drop(struct wlr_buffer *buffer) {
	if (buffer == NULL) {
		return;
	}

	assert(!buffer->dropped);
	buffer->dropped = true;
	buffer_consider_destroy(buffer);
}

struct wlr_buffer *wlr_buffer_lock(struct wlr_buffer *buffer) {
	buffer->n_locks++;
	return buffer;
}

void wlr_buffer_unlock(struct wlr_buffer *buffer) {
	if (buffer == NULL) {
		return;
	}

	assert(buffer->n_locks > 0);
	buffer->n_locks--;

	if (buffer->n_locks == 0) {
		wl_signal_emit_mutable(&buffer->events.release, NULL);
	}

	buffer_consider_destroy(buffer);
}

bool wlr_buffer_get_dmabuf(struct wlr_buffer *buffer,
		struct wlr_dmabuf_attributes *attribs) {
	if (!buffer->impl->get_dmabuf) {
		return false;
	}
	return buffer->impl->get_dmabuf(buffer, attribs);
}

bool wlr_buffer_begin_data_ptr_access(struct wlr_buffer *buffer, uint32_t flags,
		void **data, uint32_t *format, size_t *stride) {
	assert(!buffer->accessing_data_ptr);
	if (!buffer->impl->begin_data_ptr_access) {
		return false;
	}
	if (!buffer->impl->begin_data_ptr_access(buffer, flags, data, format, stride)) {
		return false;
	}
	buffer->accessing_data_ptr = true;
	return true;
}

void wlr_buffer_end_data_ptr_access(struct wlr_buffer *buffer) {
	assert(buffer->accessing_data_ptr);
	buffer->impl->end_data_ptr_access(buffer);
	buffer->accessing_data_ptr = false;
}

bool wlr_buffer_get_shm(struct wlr_buffer *buffer,
		struct wlr_shm_attributes *attribs) {
	if (!buffer->impl->get_shm) {
		return false;
	}
	return buffer->impl->get_shm(buffer, attribs);
}

bool wlr_buffer_is_opaque(struct wlr_buffer *buffer) {
	void *data;
	uint32_t format;
	size_t stride;
	struct wlr_dmabuf_attributes dmabuf;
	struct wlr_shm_attributes shm;
	if (wlr_buffer_get_dmabuf(buffer, &dmabuf)) {
		format = dmabuf.format;
	} else if (wlr_buffer_get_shm(buffer, &shm)) {
		format = shm.format;
	} else if (wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ, &data, &format, &stride)) {
		bool opaque = false;
		if (buffer->width == 1 && buffer->height == 1 && format == DRM_FORMAT_ARGB8888) {
			// Special case for single-pixel-buffer-v1
			const uint8_t *argb8888 = data; // little-endian byte order
			opaque = argb8888[3] == 0xFF;
		}
		wlr_buffer_end_data_ptr_access(buffer);
		if (opaque) {
			return true;
		}
	} else {
		return false;
	}

	return !pixel_format_has_alpha(format);
}
