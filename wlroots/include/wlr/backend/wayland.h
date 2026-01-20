#ifndef WLR_BACKEND_WAYLAND_H
#define WLR_BACKEND_WAYLAND_H

#include <stdbool.h>
#include <wayland-client-protocol.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/types/wlr_output.h>

struct wlr_input_device;

/**
 * Creates a new Wayland backend. This backend will be created with no outputs;
 * you must use wlr_wl_output_create() to add them.
 *
 * The remote_display argument is an existing libwayland-client struct wl_display
 * to use. Leave it NULL to create a new connection to the compositor.
 */
struct wlr_backend *wlr_wl_backend_create(struct wl_event_loop *loop,
		struct wl_display *remote_display);

/**
 * Returns the remote struct wl_display used by the Wayland backend.
 */
struct wl_display *wlr_wl_backend_get_remote_display(struct wlr_backend *backend);

/**
 * Adds a new output to this backend.
 *
 * This creates a new xdg_toplevel in the parent Wayland compositor.
 *
 * You may remove outputs by destroying them.
 *
 * Note that if called before initializing the backend, this will return NULL
 * and your outputs will be created during initialization (and given to you via
 * the new_output signal).
 */
struct wlr_output *wlr_wl_output_create(struct wlr_backend *backend);

/**
 * Create a new output from an existing struct wl_surface.
 */
struct wlr_output *wlr_wl_output_create_from_surface(struct wlr_backend *backend,
		struct wl_surface *surface);

/**
 * Check whether the provided backend is a Wayland backend.
 */
bool wlr_backend_is_wl(struct wlr_backend *backend);

/**
 * Check whether the provided input device is a Wayland input device.
 */
bool wlr_input_device_is_wl(struct wlr_input_device *device);

/**
 * Check whether the provided output device is a Wayland output device.
 */
bool wlr_output_is_wl(struct wlr_output *output);

/**
 * Sets the title of a struct wlr_output which is a Wayland toplevel.
 */
void wlr_wl_output_set_title(struct wlr_output *output, const char *title);

/**
 * Sets the app_id of a struct wlr_output which is a Wayland toplevel.
 */
void wlr_wl_output_set_app_id(struct wlr_output *output, const char *app_id);

/**
 * Returns the remote struct wl_surface used by the Wayland output.
 */
struct wl_surface *wlr_wl_output_get_surface(struct wlr_output *output);

#endif
