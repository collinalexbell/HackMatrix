/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_SHM_H
#define WLR_TYPES_WLR_SHM_H

#include <wayland-server-core.h>

struct wlr_renderer;

/**
 * Shared memory buffer interface.
 *
 * The buffers created via this interface are not safe to use from different
 * threads.
 *
 * Currently, accessing two buffers concurrently via
 * wlr_buffer_begin_data_ptr_access() will return an error.
 */
struct wlr_shm {
	struct wl_global *global;

	struct {
		uint32_t *formats;
		size_t formats_len;

		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

/**
 * Create the wl_shm global.
 *
 * Compositors using struct wlr_renderer should use wlr_shm_create_with_renderer()
 * instead.
 */
struct wlr_shm *wlr_shm_create(struct wl_display *display, uint32_t version,
	const uint32_t *formats, size_t formats_len);

/**
 * Create the wl_shm global.
 *
 * The pixel formats advertised to clients are taken from the struct wlr_renderer.
 */
struct wlr_shm *wlr_shm_create_with_renderer(struct wl_display *display,
	uint32_t version, struct wlr_renderer *renderer);

#endif
