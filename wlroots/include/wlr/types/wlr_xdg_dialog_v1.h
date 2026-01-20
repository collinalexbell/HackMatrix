/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_XDG_DIALOG_V1_H
#define WLR_TYPES_WLR_XDG_DIALOG_V1_H

#include <wayland-server-core.h>
#include <wlr/util/addon.h>

struct wlr_xdg_dialog_v1 {
	struct wl_resource *resource;
	struct wlr_xdg_toplevel *xdg_toplevel;

	bool modal;

	struct {
		struct wl_signal destroy;
		// Corresponds to both xdg_dialog_v1.set_modal and xdg_dialog_v1.unset_modal
		struct wl_signal set_modal;
	} events;

	struct {
		struct wlr_addon surface_addon;

		struct wl_listener xdg_toplevel_destroy;
	} WLR_PRIVATE;
};

struct wlr_xdg_wm_dialog_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
		struct wl_signal new_dialog; // struct wlr_xdg_dialog_v1
	} events;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_xdg_wm_dialog_v1 *wlr_xdg_wm_dialog_v1_create(struct wl_display *display,
		uint32_t version);

/**
 * Get a struct wlr_xdg_dialog_v1 from a struct wlr_xdg_toplevel.
 *
 * Returns NULL if there's no xdg_dialog_v1 associated with the xdg toplevel.
 */
struct wlr_xdg_dialog_v1 *wlr_xdg_dialog_v1_try_from_wlr_xdg_toplevel(
		struct wlr_xdg_toplevel *xdg_toplevel);

#endif
