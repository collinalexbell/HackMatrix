/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_LINUX_DRM_SYNCOBJ_V1_H
#define WLR_TYPES_WLR_LINUX_DRM_SYNCOBJ_V1_H

#include <wayland-server-core.h>
#include <wlr/util/addon.h>

struct wlr_buffer;
struct wlr_surface;

struct wlr_linux_drm_syncobj_surface_v1_state {
	struct wlr_drm_syncobj_timeline *acquire_timeline;
	uint64_t acquire_point;

	struct wlr_drm_syncobj_timeline *release_timeline;
	uint64_t release_point;
};

struct wlr_linux_drm_syncobj_manager_v1 {
	struct wl_global *global;

	struct {
		int drm_fd;

		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

/**
 * Advertise explicit synchronization support to clients.
 *
 * The compositor must be prepared to handle fences coming from clients and to
 * send release fences correctly. In particular, both the renderer and the
 * backend need to support explicit synchronization.
 */
struct wlr_linux_drm_syncobj_manager_v1 *wlr_linux_drm_syncobj_manager_v1_create(
	struct wl_display *display, uint32_t version, int drm_fd);

struct wlr_linux_drm_syncobj_surface_v1_state *wlr_linux_drm_syncobj_v1_get_surface_state(
	struct wlr_surface *surface);

/**
 * Signal the release point when wlr_buffer.events.release is emitted.
 *
 * Compositors unwilling to track fine-grained commit release can call this
 * helper on surface commit.
 */
bool wlr_linux_drm_syncobj_v1_state_signal_release_with_buffer(
	struct wlr_linux_drm_syncobj_surface_v1_state *state, struct wlr_buffer *buffer);

#endif
