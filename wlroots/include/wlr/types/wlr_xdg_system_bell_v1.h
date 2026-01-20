/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_XDG_SYSTEM_BELL_V1_H
#define WLR_TYPES_WLR_XDG_SYSTEM_BELL_V1_H

#include <wayland-server-core.h>

struct wlr_xdg_system_bell_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
		struct wl_signal ring; // struct wlr_xdg_system_bell_v1_ring_event
	} events;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_xdg_system_bell_v1_ring_event {
	struct wl_client *client;
	struct wlr_surface *surface; // May be NULL
};

struct wlr_xdg_system_bell_v1 *wlr_xdg_system_bell_v1_create(struct wl_display *display,
		uint32_t version);

#endif
