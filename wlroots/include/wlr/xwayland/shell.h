/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_XWAYLAND_SHELL_H
#define WLR_XWAYLAND_SHELL_H

#include <stdbool.h>
#include <wayland-server-core.h>

/**
 * The Xwayland shell.
 *
 * This is a shell only exposed to Xwayland.
 */
struct wlr_xwayland_shell_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
		struct wl_signal new_surface; // struct wlr_xwayland_surface_v1
	} events;

	struct {
		struct wl_client *client;
		struct wl_list surfaces; // wlr_xwayland_surface_v1.link

		struct wl_listener display_destroy;
		struct wl_listener client_destroy;
	} WLR_PRIVATE;
};

/**
 * An Xwayland shell surface.
 */
struct wlr_xwayland_surface_v1 {
	struct wlr_surface *surface;
	uint64_t serial;

	struct {
		struct wl_resource *resource;
		struct wl_list link;
		struct wlr_xwayland_shell_v1 *shell;
		bool added;
	} WLR_PRIVATE;
};

/**
 * Create the xwayland_shell_v1 global.
 *
 * Compositors should add a global filter (see wl_display_set_global_filter())
 * to only expose this global to Xwayland clients.
 */
struct wlr_xwayland_shell_v1 *wlr_xwayland_shell_v1_create(
	struct wl_display *display, uint32_t version);

/**
 * Destroy the xwayland_shell_v1 global.
 */
void wlr_xwayland_shell_v1_destroy(struct wlr_xwayland_shell_v1 *shell);

/**
 * Allow a client to bind to the xwayland_shell_v1 global.
 */
void wlr_xwayland_shell_v1_set_client(struct wlr_xwayland_shell_v1 *shell,
	struct wl_client *client);

/**
 * Get a Wayland surface from an xwayland_shell_v1 serial.
 *
 * Returns NULL if the serial hasn't been associated with any surface.
 */
struct wlr_surface *wlr_xwayland_shell_v1_surface_from_serial(
	struct wlr_xwayland_shell_v1 *shell, uint64_t serial);

#endif
