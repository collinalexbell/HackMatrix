/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_TEARING_CONTROL_MANAGER_V1_H
#define WLR_TYPES_WLR_TEARING_CONTROL_MANAGER_V1_H

#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-protocols/tearing-control-v1-enum.h>
#include <wlr/types/wlr_compositor.h>

struct wlr_tearing_control_v1 {
	struct wl_client *client;
	struct wl_list link;
	struct wl_resource *resource;

	enum wp_tearing_control_v1_presentation_hint current, pending;

	struct {
		struct wl_signal set_hint;
		struct wl_signal destroy;
	} events;

	struct wlr_surface *surface;

	struct {
		enum wp_tearing_control_v1_presentation_hint previous;
		struct wlr_addon addon;
		struct wlr_surface_synced synced;

		struct wl_listener surface_commit;
	} WLR_PRIVATE;
};

struct wlr_tearing_control_manager_v1 {
	struct wl_global *global;

	struct wl_list surface_hints;  // wlr_tearing_control_v1.link

	struct {
		struct wl_signal new_object;  // struct wlr_tearing_control_v1*
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_tearing_control_manager_v1 *wlr_tearing_control_manager_v1_create(
	struct wl_display *display, uint32_t version);

/**
 * Returns the tearing hint for a given surface
 */
enum wp_tearing_control_v1_presentation_hint
wlr_tearing_control_manager_v1_surface_hint_from_surface(
	struct wlr_tearing_control_manager_v1 *manager,
	struct wlr_surface *surface);

#endif
