#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/xwayland/shell.h>

#include "xwayland-shell-v1-protocol.h"

#define SHELL_VERSION 1

static void destroy_resource(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct xwayland_shell_v1_interface shell_impl;
static const struct xwayland_surface_v1_interface xwl_surface_impl;

static void xwl_surface_destroy(struct wlr_xwayland_surface_v1 *xwl_surface) {
	wl_list_remove(&xwl_surface->link);
	wl_resource_set_user_data(xwl_surface->resource, NULL); // make inert
	free(xwl_surface);
}

/**
 * Get a struct wlr_xwayland_shell_v1 from a resource.
 */
static struct wlr_xwayland_shell_v1 *shell_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&xwayland_shell_v1_interface, &shell_impl));
	return wl_resource_get_user_data(resource);
}

/**
 * Get a struct wlr_xwayland_surface_v1 from a resource.
 *
 * Returns NULL if the Xwayland surface is inert.
 */
static struct wlr_xwayland_surface_v1 *xwl_surface_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&xwayland_surface_v1_interface, &xwl_surface_impl));
	return wl_resource_get_user_data(resource);
}

static void xwl_surface_role_commit(struct wlr_surface *surface) {
	struct wlr_xwayland_surface_v1 *xwl_surface = xwl_surface_from_resource(surface->role_resource);
	if (xwl_surface == NULL) {
		return;
	}

	if (xwl_surface->serial != 0 && !xwl_surface->added) {
		xwl_surface->added = true;
		wl_signal_emit_mutable(&xwl_surface->shell->events.new_surface,
			xwl_surface);
	}
}

static void xwl_surface_role_destroy(struct wlr_surface *surface) {
	struct wlr_xwayland_surface_v1 *xwl_surface = xwl_surface_from_resource(surface->role_resource);
	if (xwl_surface == NULL) {
		return;
	}

	xwl_surface_destroy(xwl_surface);
}

static const struct wlr_surface_role xwl_surface_role = {
	.name = "xwayland_surface_v1",
	.commit = xwl_surface_role_commit,
	.destroy = xwl_surface_role_destroy,
};

static void xwl_surface_handle_set_serial(struct wl_client *client,
		struct wl_resource *resource, uint32_t serial_lo, uint32_t serial_hi) {
	struct wlr_xwayland_surface_v1 *xwl_surface =
		xwl_surface_from_resource(resource);
	if (xwl_surface == NULL) {
		return;
	}

	if (xwl_surface->serial != 0) {
		wl_resource_post_error(resource,
			XWAYLAND_SURFACE_V1_ERROR_ALREADY_ASSOCIATED,
			"xwayland_surface_v1 is already associated with another X11 serial");
		return;
	}

	xwl_surface->serial = ((uint64_t)serial_hi << 32) | serial_lo;
}

static const struct xwayland_surface_v1_interface xwl_surface_impl = {
	.destroy = destroy_resource,
	.set_serial = xwl_surface_handle_set_serial,
};

static void shell_handle_get_xwayland_surface(struct wl_client *client,
		struct wl_resource *shell_resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wlr_xwayland_shell_v1 *shell = shell_from_resource(shell_resource);
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);

	struct wlr_xwayland_surface_v1 *xwl_surface = calloc(1, sizeof(*xwl_surface));
	if (xwl_surface == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	if (!wlr_surface_set_role(surface, &xwl_surface_role,
			shell_resource, XWAYLAND_SHELL_V1_ERROR_ROLE)) {
		free(xwl_surface);
		return;
	}

	xwl_surface->surface = surface;
	xwl_surface->shell = shell;

	uint32_t version = wl_resource_get_version(shell_resource);
	xwl_surface->resource = wl_resource_create(client,
		&xwayland_surface_v1_interface, version, id);
	if (xwl_surface->resource == NULL) {
		free(xwl_surface);
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(xwl_surface->resource, &xwl_surface_impl,
		xwl_surface, NULL);

	wl_list_insert(&shell->surfaces, &xwl_surface->link);

	wlr_surface_set_role_object(surface, xwl_surface->resource);
}

static const struct xwayland_shell_v1_interface shell_impl = {
	.destroy = destroy_resource,
	.get_xwayland_surface = shell_handle_get_xwayland_surface,
};

static void shell_bind(struct wl_client *client, void *data, uint32_t version,
		uint32_t id) {
	struct wlr_xwayland_shell_v1 *shell = data;

	if (client != shell->client) {
		wl_client_post_implementation_error(client,
			"Permission denied to bind to %s", xwayland_shell_v1_interface.name);
		return;
	}

	struct wl_resource *resource = wl_resource_create(client,
		&xwayland_shell_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &shell_impl, shell, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_shell_v1 *shell =
		wl_container_of(listener, shell, display_destroy);
	wlr_xwayland_shell_v1_destroy(shell);
}

struct wlr_xwayland_shell_v1 *wlr_xwayland_shell_v1_create(
		struct wl_display *display, uint32_t version) {
	assert(version <= SHELL_VERSION);

	struct wlr_xwayland_shell_v1 *shell = calloc(1, sizeof(*shell));
	if (shell == NULL) {
		return NULL;
	}

	shell->global = wl_global_create(display, &xwayland_shell_v1_interface,
		version, shell, shell_bind);
	if (shell->global == NULL) {
		free(shell);
		return NULL;
	}

	wl_list_init(&shell->surfaces);

	wl_signal_init(&shell->events.new_surface);
	wl_signal_init(&shell->events.destroy);

	shell->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &shell->display_destroy);

	wl_list_init(&shell->client_destroy.link);

	return shell;
}

void wlr_xwayland_shell_v1_destroy(struct wlr_xwayland_shell_v1 *shell) {
	if (shell == NULL) {
		return;
	}

	wl_signal_emit_mutable(&shell->events.destroy, NULL);

	assert(wl_list_empty(&shell->events.new_surface.listener_list));
	assert(wl_list_empty(&shell->events.destroy.listener_list));

	struct wlr_xwayland_surface_v1 *xwl_surface, *tmp;
	wl_list_for_each_safe(xwl_surface, tmp, &shell->surfaces, link) {
		xwl_surface_destroy(xwl_surface);
	}

	wl_list_remove(&shell->display_destroy.link);
	wl_list_remove(&shell->client_destroy.link);
	wl_global_destroy(shell->global);
	free(shell);
}

static void shell_handle_client_destroy(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_shell_v1 *shell =
		wl_container_of(listener, shell, client_destroy);
	wlr_xwayland_shell_v1_set_client(shell, NULL);
}

void wlr_xwayland_shell_v1_set_client(struct wlr_xwayland_shell_v1 *shell,
		struct wl_client *client) {
	wl_list_remove(&shell->client_destroy.link);
	shell->client = client;
	if (client != NULL) {
		shell->client_destroy.notify = shell_handle_client_destroy;
		wl_client_add_destroy_listener(client, &shell->client_destroy);
	} else {
		wl_list_init(&shell->client_destroy.link);
	}
}

struct wlr_surface *wlr_xwayland_shell_v1_surface_from_serial(
		struct wlr_xwayland_shell_v1 *shell, uint64_t serial) {
	struct wlr_xwayland_surface_v1 *xwl_surface;
	wl_list_for_each(xwl_surface, &shell->surfaces, link) {
		if (xwl_surface->serial == serial) {
			return xwl_surface->surface;
		}
	}
	return NULL;
}
