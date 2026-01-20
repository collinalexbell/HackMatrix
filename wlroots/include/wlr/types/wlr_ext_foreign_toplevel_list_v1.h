/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_FOREIGN_TOPLEVEL_LIST_V1_H
#define WLR_TYPES_WLR_FOREIGN_TOPLEVEL_LIST_V1_H

#include <wayland-server-core.h>

struct wlr_ext_foreign_toplevel_list_v1 {
	struct wl_global *global;
	struct wl_list resources; // wl_resource_get_link()
	struct wl_list toplevels; // ext_foreign_toplevel_handle_v1.link

	struct {
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_ext_foreign_toplevel_handle_v1 {
	struct wlr_ext_foreign_toplevel_list_v1 *list;
	struct wl_list resources; // wl_resource_get_link()
	struct wl_list link; // wlr_ext_foreign_toplevel_list_v1.toplevels

	char *title;
	char *app_id;
	char *identifier;

	struct {
		struct wl_signal destroy;
	} events;

	void *data;
};

struct wlr_ext_foreign_toplevel_handle_v1_state {
	const char *title;
	const char *app_id;
};

struct wlr_ext_foreign_toplevel_list_v1 *wlr_ext_foreign_toplevel_list_v1_create(
	struct wl_display *display, uint32_t version);

struct wlr_ext_foreign_toplevel_handle_v1 *wlr_ext_foreign_toplevel_handle_v1_create(
	struct wlr_ext_foreign_toplevel_list_v1 *list,
	const struct wlr_ext_foreign_toplevel_handle_v1_state *state);

/**
 * Destroy the given toplevel handle, sending the closed event to any
 * client.
 */
void wlr_ext_foreign_toplevel_handle_v1_destroy(
	struct wlr_ext_foreign_toplevel_handle_v1 *toplevel);

void wlr_ext_foreign_toplevel_handle_v1_update_state(
	struct wlr_ext_foreign_toplevel_handle_v1 *toplevel,
	const struct wlr_ext_foreign_toplevel_handle_v1_state *state);

struct wlr_ext_foreign_toplevel_handle_v1 *wlr_ext_foreign_toplevel_handle_v1_from_resource(
	struct wl_resource *resource);

#endif
