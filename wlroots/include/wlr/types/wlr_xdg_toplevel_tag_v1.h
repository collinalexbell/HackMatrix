/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_XDG_TOPLEVEL_TAG_V1_H
#define WLR_TYPES_WLR_XDG_TOPLEVEL_TAG_V1_H

#include <wayland-server-core.h>

struct wlr_xdg_toplevel_tag_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal set_tag; // struct wlr_xdg_toplevel_tag_manager_v1_set_tag_event
		struct wl_signal set_description; // struct wlr_xdg_toplevel_tag_manager_v1_set_description_event
		struct wl_signal destroy;
	} events;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_xdg_toplevel_tag_manager_v1_set_tag_event {
	struct wlr_xdg_toplevel *toplevel;
	const char *tag;
};

struct wlr_xdg_toplevel_tag_manager_v1_set_description_event {
	struct wlr_xdg_toplevel *toplevel;
	const char *description;
};

struct wlr_xdg_toplevel_tag_manager_v1 *wlr_xdg_toplevel_tag_manager_v1_create(
	struct wl_display *display, uint32_t version);

#endif
