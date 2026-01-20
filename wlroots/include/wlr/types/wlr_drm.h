/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_DRM_H
#define WLR_TYPES_WLR_DRM_H

#include <wayland-server-protocol.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/types/wlr_buffer.h>

struct wlr_renderer;

struct wlr_drm_buffer {
	struct wlr_buffer base;

	struct wl_resource *resource; // can be NULL if the client destroyed it
	struct wlr_dmabuf_attributes dmabuf;

	struct {
		struct wl_listener release;
	} WLR_PRIVATE;
};

/**
 * A stub implementation of Mesa's wl_drm protocol.
 *
 * It only implements the minimum necessary for modern clients to behave
 * properly. In particular, flink handles are left unimplemented.
 *
 * Deprecated: this protocol is legacy and superseded by linux-dmabuf. The
 * implementation will be dropped in a future wlroots version.
 */
struct wlr_drm {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	struct {
		char *node_name;
		struct wlr_drm_format_set formats;

		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_drm_buffer *wlr_drm_buffer_try_from_resource(
	struct wl_resource *resource);

struct wlr_drm *wlr_drm_create(struct wl_display *display,
	struct wlr_renderer *renderer);

#endif
