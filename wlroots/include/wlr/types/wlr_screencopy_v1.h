/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_SCREENCOPY_V1_H
#define WLR_TYPES_WLR_SCREENCOPY_V1_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/util/box.h>

/**
 * Deprecated: this protocol is deprecated and superseded by ext-image-copy-capture-v1.
 * The implementation will be dropped in a future wlroots version.
 *
 * Consider using `wlr_ext_image_capture_source_v1` instead.
 */

struct wlr_screencopy_manager_v1 {
	struct wl_global *global;
	struct wl_list frames; // wlr_screencopy_frame_v1.link

	struct {
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_screencopy_v1_client {
	int ref;
	struct wlr_screencopy_manager_v1 *manager;
	struct wl_list damages;
};

struct wlr_screencopy_frame_v1 {
	struct wl_resource *resource;
	struct wlr_screencopy_v1_client *client;
	struct wl_list link; // wlr_screencopy_manager_v1.frames

	uint32_t shm_format, dmabuf_format; // DRM format codes
	struct wlr_box box;
	int shm_stride;

	bool overlay_cursor, cursor_locked;

	bool with_damage;

	enum wlr_buffer_cap buffer_cap;
	struct wlr_buffer *buffer;

	struct wlr_output *output;

	void *data;

	struct {
		struct wl_listener output_commit;
		struct wl_listener output_destroy;
	} WLR_PRIVATE;
};

struct wlr_screencopy_manager_v1 *wlr_screencopy_manager_v1_create(
	struct wl_display *display);

#endif
