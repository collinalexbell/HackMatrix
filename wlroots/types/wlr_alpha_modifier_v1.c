#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/util/addon.h>
#include "alpha-modifier-v1-protocol.h"

#define ALPHA_MODIFIER_VERSION 1

struct wlr_alpha_modifier_surface_v1 {
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wlr_addon addon;
	struct wlr_surface_synced synced;
	struct wlr_alpha_modifier_surface_v1_state pending, current;
};

static const struct wp_alpha_modifier_surface_v1_interface surface_impl;

// Returns NULL if the wl_surface was destroyed
static struct wlr_alpha_modifier_surface_v1 *surface_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wp_alpha_modifier_surface_v1_interface,
		&surface_impl));
	return wl_resource_get_user_data(resource);
}

static void surface_destroy(struct wlr_alpha_modifier_surface_v1 *surface) {
	if (surface == NULL) {
		return;
	}
	wlr_surface_synced_finish(&surface->synced);
	wlr_addon_finish(&surface->addon);
	wl_resource_set_user_data(surface->resource, NULL);
	free(surface);
}

static void surface_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_alpha_modifier_surface_v1 *surface = surface_from_resource(resource);
	surface_destroy(surface);
}

static void surface_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void surface_handle_set_multiplier(struct wl_client *client,
		struct wl_resource *resource, uint32_t multiplier) {
	struct wlr_alpha_modifier_surface_v1 *surface = surface_from_resource(resource);
	if (surface == NULL) {
		wl_resource_post_error(resource,
			WP_ALPHA_MODIFIER_SURFACE_V1_ERROR_NO_SURFACE,
			"The wl_surface object has been destroyed");
		return;
	}
	surface->pending.multiplier = (double)multiplier / UINT32_MAX;
}

static const struct wp_alpha_modifier_surface_v1_interface surface_impl = {
	.destroy = surface_handle_destroy,
	.set_multiplier = surface_handle_set_multiplier,
};

static void surface_synced_init_state(void *_state) {
	struct wlr_alpha_modifier_surface_v1_state *state = _state;
	state->multiplier = 1;
}

static const struct wlr_surface_synced_impl surface_synced_impl = {
	.state_size = sizeof(struct wlr_alpha_modifier_surface_v1_state),
	.init_state = surface_synced_init_state,
};

static void surface_addon_destroy(struct wlr_addon *addon) {
	struct wlr_alpha_modifier_surface_v1 *surface = wl_container_of(addon, surface, addon);
	surface_destroy(surface);
}

static const struct wlr_addon_interface surface_addon_impl = {
	.name = "wp_alpha_modifier_surface_v1",
	.destroy = surface_addon_destroy,
};

static struct wlr_alpha_modifier_surface_v1 *surface_from_wlr_surface(
		struct wlr_surface *wlr_surface) {
	struct wlr_addon *addon = wlr_addon_find(&wlr_surface->addons, NULL, &surface_addon_impl);
	if (addon == NULL) {
		return NULL;
	}
	struct wlr_alpha_modifier_surface_v1 *surface = wl_container_of(addon, surface, addon);
	return surface;
}

static void manager_handle_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void manager_handle_get_surface(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wlr_surface *wlr_surface = wlr_surface_from_resource(surface_resource);

	if (surface_from_wlr_surface(wlr_surface) != NULL) {
		wl_resource_post_error(manager_resource,
			WP_ALPHA_MODIFIER_V1_ERROR_ALREADY_CONSTRUCTED,
			"The wl_surface object already has a wp_alpha_modifier_surface_v1 object");
		return;
	}

	struct wlr_alpha_modifier_surface_v1 *surface = calloc(1, sizeof(*surface));
	if (surface == NULL) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	if (!wlr_surface_synced_init(&surface->synced, wlr_surface,
			&surface_synced_impl, &surface->pending, &surface->current)) {
		free(surface);
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	uint32_t version = wl_resource_get_version(manager_resource);
	surface->resource = wl_resource_create(client,
		&wp_alpha_modifier_surface_v1_interface, version, id);
	if (surface->resource == NULL) {
		wlr_surface_synced_finish(&surface->synced);
		free(surface);
		wl_resource_post_no_memory(manager_resource);
		return;
	}
	wl_resource_set_implementation(surface->resource, &surface_impl,
		surface, surface_handle_resource_destroy);

	surface->surface = wlr_surface;
	wlr_addon_init(&surface->addon, &wlr_surface->addons, NULL, &surface_addon_impl);
}

static const struct wp_alpha_modifier_v1_interface manager_impl = {
	.destroy = manager_handle_destroy,
	.get_surface = manager_handle_get_surface,
};

static void manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wl_resource *resource = wl_resource_create(client,
		&wp_alpha_modifier_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, NULL, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_alpha_modifier_v1 *manager = wl_container_of(listener, manager, display_destroy);
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wlr_alpha_modifier_v1 *wlr_alpha_modifier_v1_create(struct wl_display *display) {
	struct wlr_alpha_modifier_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display, &wp_alpha_modifier_v1_interface,
		ALPHA_MODIFIER_VERSION, NULL, manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}

const struct wlr_alpha_modifier_surface_v1_state *wlr_alpha_modifier_v1_get_surface_state(
		struct wlr_surface *wlr_surface) {
	struct wlr_alpha_modifier_surface_v1 *surface = surface_from_wlr_surface(wlr_surface);
	if (surface == NULL) {
		return NULL;
	}
	return &surface->current;
}
