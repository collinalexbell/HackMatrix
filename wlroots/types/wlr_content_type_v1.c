#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_content_type_v1.h>
#include <wlr/types/wlr_compositor.h>

#include "content-type-v1-protocol.h"

#define CONTENT_TYPE_VERSION 1

struct wlr_content_type_v1_surface {
	struct wl_resource *resource;
	struct wlr_addon addon;
	enum wp_content_type_v1_type pending, current;

	struct wlr_surface_synced synced;
};

static void resource_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wp_content_type_v1_interface content_type_surface_impl;
static const struct wp_content_type_manager_v1_interface manager_impl;

// Returns NULL if the resource is inert
static struct wlr_content_type_v1_surface *content_type_surface_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_content_type_v1_interface, &content_type_surface_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_content_type_manager_v1 *manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_content_type_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

static void content_type_surface_handle_set_content_type(struct wl_client *client,
		struct wl_resource *resource, uint32_t type) {
	struct wlr_content_type_v1_surface *content_type_surface =
		content_type_surface_from_resource(resource);
	if (content_type_surface == NULL) {
		return;
	}
	content_type_surface->pending = type;
}

static const struct wp_content_type_v1_interface content_type_surface_impl = {
	.destroy = resource_handle_destroy,
	.set_content_type = content_type_surface_handle_set_content_type,
};

static void content_type_surface_destroy(
		struct wlr_content_type_v1_surface *content_type_surface) {
	if (content_type_surface == NULL) {
		return;
	}
	wlr_addon_finish(&content_type_surface->addon);
	wlr_surface_synced_finish(&content_type_surface->synced);
	wl_resource_set_user_data(content_type_surface->resource, NULL);
	free(content_type_surface);
}

static void surface_addon_destroy(struct wlr_addon *addon) {
	struct wlr_content_type_v1_surface *content_type_surface =
		wl_container_of(addon, content_type_surface, addon);
	content_type_surface_destroy(content_type_surface);
}

static const struct wlr_addon_interface surface_addon_impl = {
	.name = "wp_content_type_v1",
	.destroy = surface_addon_destroy,
};

static const struct wlr_surface_synced_impl surface_synced_impl = {
	.state_size = sizeof(enum wp_content_type_v1_type),
};

static void content_type_surface_handle_resource_destroy(
		struct wl_resource *resource) {
	struct wlr_content_type_v1_surface *content_type_surface =
		content_type_surface_from_resource(resource);
	content_type_surface_destroy(content_type_surface);
}

static void manager_handle_get_surface_content_type(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wlr_content_type_manager_v1 *manager =
		manager_from_resource(manager_resource);
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);

	if (wlr_addon_find(&surface->addons, manager, &surface_addon_impl) != NULL) {
		wl_resource_post_error(manager_resource,
			WP_CONTENT_TYPE_MANAGER_V1_ERROR_ALREADY_CONSTRUCTED,
			"wp_content_type_v1 already constructed for this surface");
		return;
	}

	struct wlr_content_type_v1_surface *content_type_surface =
		calloc(1, sizeof(*content_type_surface));
	if (content_type_surface == NULL) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	if (!wlr_surface_synced_init(&content_type_surface->synced, surface,
			&surface_synced_impl, &content_type_surface->pending,
			&content_type_surface->current)) {
		free(content_type_surface);
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	uint32_t version = wl_resource_get_version(manager_resource);
	content_type_surface->resource = wl_resource_create(client,
		&wp_content_type_v1_interface, version, id);
	if (content_type_surface->resource == NULL) {
		wlr_surface_synced_finish(&content_type_surface->synced);
		free(content_type_surface);
		wl_resource_post_no_memory(manager_resource);
		return;
	}
	wl_resource_set_implementation(content_type_surface->resource,
		&content_type_surface_impl, content_type_surface,
		content_type_surface_handle_resource_destroy);

	wlr_addon_init(&content_type_surface->addon, &surface->addons,
		manager, &surface_addon_impl);
}

static const struct wp_content_type_manager_v1_interface manager_impl = {
	.destroy = resource_handle_destroy,
	.get_surface_content_type = manager_handle_get_surface_content_type,
};

static void manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_content_type_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&wp_content_type_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, manager, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_content_type_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);

	wl_signal_emit_mutable(&manager->events.destroy, NULL);
	assert(wl_list_empty(&manager->events.destroy.listener_list));

	wl_global_destroy(manager->global);
	wl_list_remove(&manager->display_destroy.link);
	free(manager);
}

struct wlr_content_type_manager_v1 *wlr_content_type_manager_v1_create(
		struct wl_display *display, uint32_t version) {
	assert(version <= CONTENT_TYPE_VERSION);

	struct wlr_content_type_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&wp_content_type_manager_v1_interface, version, manager, manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.destroy);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}

enum wp_content_type_v1_type wlr_surface_get_content_type_v1(
		struct wlr_content_type_manager_v1 *manager, struct wlr_surface *surface) {
	struct wlr_addon *addon =
		wlr_addon_find(&surface->addons, manager, &surface_addon_impl);
	if (addon == NULL) {
		return WP_CONTENT_TYPE_V1_TYPE_NONE;
	}

	struct wlr_content_type_v1_surface *content_type_surface =
		wl_container_of(addon, content_type_surface, addon);
	return content_type_surface->current;
}
