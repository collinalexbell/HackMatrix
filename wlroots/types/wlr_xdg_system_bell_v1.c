#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_xdg_system_bell_v1.h>
#include "xdg-system-bell-v1-protocol.h"

#define XDG_SYSTEM_BELL_V1_VERSION 1

static const struct xdg_system_bell_v1_interface bell_impl;

static struct wlr_xdg_system_bell_v1 *bell_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &xdg_system_bell_v1_interface, &bell_impl));
	return wl_resource_get_user_data(resource);
}

static void bell_handle_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void bell_handle_ring(struct wl_client *client,
		struct wl_resource *bell_resource, struct wl_resource *surface_resource) {
	struct wlr_xdg_system_bell_v1 *bell = bell_from_resource(bell_resource);

	struct wlr_surface *surface = NULL;
	if (surface_resource != NULL) {
		surface = wlr_surface_from_resource(surface_resource);
	}

	struct wlr_xdg_system_bell_v1_ring_event event = {
		.client = client,
		.surface = surface,
	};
	wl_signal_emit_mutable(&bell->events.ring, &event);
}

static const struct xdg_system_bell_v1_interface bell_impl = {
	.destroy = bell_handle_destroy,
	.ring = bell_handle_ring,
};

static void bell_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_xdg_system_bell_v1 *bell = data;

	struct wl_resource *resource = wl_resource_create(client,
		&xdg_system_bell_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &bell_impl, bell, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_xdg_system_bell_v1 *bell = wl_container_of(listener, bell, display_destroy);
	wl_signal_emit_mutable(&bell->events.destroy, NULL);

	assert(wl_list_empty(&bell->events.destroy.listener_list));
	assert(wl_list_empty(&bell->events.ring.listener_list));

	wl_list_remove(&bell->display_destroy.link);
	wl_global_destroy(bell->global);
	free(bell);
}

struct wlr_xdg_system_bell_v1 *wlr_xdg_system_bell_v1_create(struct wl_display *display,
		uint32_t version) {
	assert(version <= XDG_SYSTEM_BELL_V1_VERSION);

	struct wlr_xdg_system_bell_v1 *bell = calloc(1, sizeof(*bell));
	if (bell == NULL) {
		return NULL;
	}

	bell->global = wl_global_create(display, &xdg_system_bell_v1_interface,
		version, bell, bell_bind);
	if (bell->global == NULL) {
		free(bell);
		return NULL;
	}

	bell->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &bell->display_destroy);

	wl_signal_init(&bell->events.destroy);
	wl_signal_init(&bell->events.ring);

	return bell;
}
