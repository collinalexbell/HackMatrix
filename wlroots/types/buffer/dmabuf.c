#include <assert.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_buffer.h>
#include <wlr/util/log.h>
#include "types/wlr_buffer.h"

static const struct wlr_buffer_impl dmabuf_buffer_impl;

static struct wlr_dmabuf_buffer *dmabuf_buffer_from_buffer(
		struct wlr_buffer *wlr_buffer) {
	assert(wlr_buffer->impl == &dmabuf_buffer_impl);
	struct wlr_dmabuf_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	return buffer;
}

static void dmabuf_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct wlr_dmabuf_buffer *buffer = dmabuf_buffer_from_buffer(wlr_buffer);
	wlr_buffer_finish(wlr_buffer);
	if (buffer->saved) {
		wlr_dmabuf_attributes_finish(&buffer->dmabuf);
	}
	free(buffer);
}

static bool dmabuf_buffer_get_dmabuf(struct wlr_buffer *wlr_buffer,
		struct wlr_dmabuf_attributes *dmabuf) {
	struct wlr_dmabuf_buffer *buffer = dmabuf_buffer_from_buffer(wlr_buffer);
	if (buffer->dmabuf.n_planes == 0) {
		return false;
	}
	*dmabuf = buffer->dmabuf;
	return true;
}

static const struct wlr_buffer_impl dmabuf_buffer_impl = {
	.destroy = dmabuf_buffer_destroy,
	.get_dmabuf = dmabuf_buffer_get_dmabuf,
};

struct wlr_dmabuf_buffer *dmabuf_buffer_create(
		struct wlr_dmabuf_attributes *dmabuf) {
	struct wlr_dmabuf_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return NULL;
	}
	wlr_buffer_init(&buffer->base, &dmabuf_buffer_impl,
		dmabuf->width, dmabuf->height);

	buffer->dmabuf = *dmabuf;

	return buffer;
}

bool dmabuf_buffer_drop(struct wlr_dmabuf_buffer *buffer) {
	bool ok = true;

	if (buffer->base.n_locks > 0) {
		struct wlr_dmabuf_attributes saved_dmabuf = {0};
		if (!wlr_dmabuf_attributes_copy(&saved_dmabuf, &buffer->dmabuf)) {
			wlr_log(WLR_ERROR, "Failed to save DMA-BUF");
			ok = false;
			buffer->dmabuf = (struct wlr_dmabuf_attributes){0};
		} else {
			buffer->dmabuf = saved_dmabuf;
			buffer->saved = true;
		}
	}

	wlr_buffer_drop(&buffer->base);
	return ok;
}
