/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_FRACTIONAL_SCALE_V1_H
#define WLR_TYPES_WLR_FRACTIONAL_SCALE_V1_H

#include <wayland-server-core.h>

struct wlr_surface;

struct wlr_fractional_scale_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

void wlr_fractional_scale_v1_notify_scale(
		struct wlr_surface *surface, double scale);

struct wlr_fractional_scale_manager_v1 *wlr_fractional_scale_manager_v1_create(
		struct wl_display *display, uint32_t version);

#endif
