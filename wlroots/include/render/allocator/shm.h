#ifndef RENDER_ALLOCATOR_SHM_H
#define RENDER_ALLOCATOR_SHM_H

#include <wlr/render/allocator.h>
#include <wlr/types/wlr_buffer.h>

struct wlr_shm_buffer {
	struct wlr_buffer base;
	struct wlr_shm_attributes shm;
	void *data;
	size_t size;
};

struct wlr_shm_allocator {
	struct wlr_allocator base;
};

/**
 * Creates a new shared memory allocator.
 */
struct wlr_allocator *wlr_shm_allocator_create(void);

#endif
