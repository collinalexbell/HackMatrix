/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_FIXES_H
#define WLR_TYPES_WLR_FIXES_H

#include <wayland-server-core.h>

struct wlr_fixes {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_fixes *wlr_fixes_create(struct wl_display *display, uint32_t version);

#endif
