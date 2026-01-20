/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_ALPHA_MODIFIER_V1_H
#define WLR_TYPES_WLR_ALPHA_MODIFIER_V1_H

#include <wayland-server-core.h>

struct wlr_surface;

struct wlr_alpha_modifier_surface_v1_state {
	double multiplier; // between 0 and 1
};

struct wlr_alpha_modifier_v1 {
	struct wl_global *global;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_alpha_modifier_v1 *wlr_alpha_modifier_v1_create(struct wl_display *display);

const struct wlr_alpha_modifier_surface_v1_state *wlr_alpha_modifier_v1_get_surface_state(
	struct wlr_surface *surface);

#endif
