/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_CONTENT_TYPE_V1_H
#define WLR_TYPES_WLR_CONTENT_TYPE_V1_H

#include <wayland-server-core.h>
#include <wayland-protocols/content-type-v1-enum.h>

struct wlr_surface;

struct wlr_content_type_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_content_type_manager_v1 *wlr_content_type_manager_v1_create(
	struct wl_display *display, uint32_t version);
enum wp_content_type_v1_type wlr_surface_get_content_type_v1(
	struct wlr_content_type_manager_v1 *manager, struct wlr_surface *surface);

#endif
