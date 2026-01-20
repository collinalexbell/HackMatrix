#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/util/log.h>

#include "fractional-scale-v1-protocol.h"

#define FRACTIONAL_SCALE_VERSION 1

struct wlr_fractional_scale_v1 {
	struct wl_resource *resource;
	struct wlr_addon addon;

	// Used for dummy objects to store scale
	double scale;
};

static const struct wp_fractional_scale_v1_interface fractional_scale_interface;

static struct wlr_fractional_scale_v1 *fractional_scale_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_fractional_scale_v1_interface, &fractional_scale_interface));
	return wl_resource_get_user_data(resource);
}

static const struct wp_fractional_scale_manager_v1_interface scale_manager_interface;

static void fractional_scale_destroy(struct wlr_fractional_scale_v1 *info) {
	if (info == NULL) {
		return;
	}

	// If this is a dummy object then we do not have a wl_resource
	if (info->resource != NULL) {
		wl_resource_set_user_data(info->resource, NULL);
	}
	wlr_addon_finish(&info->addon);
	free(info);
}

static void fractional_scale_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_fractional_scale_v1 *info = fractional_scale_from_resource(resource);
	fractional_scale_destroy(info);
}

static void fractional_scale_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wp_fractional_scale_v1_interface fractional_scale_interface = {
	.destroy = fractional_scale_handle_destroy,
};

static void fractional_scale_addon_destroy(struct wlr_addon *addon) {
	struct wlr_fractional_scale_v1 *info = wl_container_of(addon, info, addon);
	fractional_scale_destroy(info);
}

static const struct wlr_addon_interface fractional_scale_addon_impl = {
	.name = "wlr_fractional_scale_v1",
	.destroy = fractional_scale_addon_destroy,
};

static uint32_t double_to_v120(double d) {
	return round(d * 120);
}

void wlr_fractional_scale_v1_notify_scale(struct wlr_surface *surface, double scale) {
	struct wlr_addon *addon = wlr_addon_find(&surface->addons,
		NULL, &fractional_scale_addon_impl);

	if (addon == NULL) {
		// Create a dummy object to store the scale
		struct wlr_fractional_scale_v1 *info = calloc(1, sizeof(*info));
		if (info == NULL) {
			return;
		}

		wlr_addon_init(&info->addon, &surface->addons, NULL, &fractional_scale_addon_impl);
		info->scale = scale;
		return;
	}

	struct wlr_fractional_scale_v1 *info = wl_container_of(addon, info, addon);
	if (info->scale == scale) {
		return;
	}

	info->scale = scale;

	if (!info->resource) {
		// Update existing dummy object
		return;
	}

	wp_fractional_scale_v1_send_preferred_scale(info->resource, double_to_v120(scale));
}

static void handle_get_fractional_scale(struct wl_client *client,
		struct wl_resource *mgr_resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);
	struct wlr_fractional_scale_v1 *info = NULL;

	// If no fractional_scale object had been created but scale had been set, then
	// there will be a dummy object for us to fill out with a resource. Check if
	// that's the case.
	struct wlr_addon *addon = wlr_addon_find(&surface->addons, NULL, &fractional_scale_addon_impl);

	if (addon != NULL) {
		info = wl_container_of(addon, info, addon);
		if (info->resource != NULL) {
			wl_resource_post_error(mgr_resource,
				WP_FRACTIONAL_SCALE_MANAGER_V1_ERROR_FRACTIONAL_SCALE_EXISTS,
				"a surface scale object for that surface already exists");
			return;
		}
	}

	if (info == NULL) {
		info = calloc(1, sizeof(*info));
		if (info == NULL) {
			wl_client_post_no_memory(client);
			return;
		}

		wlr_addon_init(&info->addon, &surface->addons, NULL, &fractional_scale_addon_impl);
	}

	uint32_t version = wl_resource_get_version(mgr_resource);
	info->resource = wl_resource_create(client, &wp_fractional_scale_v1_interface, version, id);
	if (info->resource == NULL) {
		wl_client_post_no_memory(client);
		fractional_scale_destroy(info);
		return;
	}
	wl_resource_set_implementation(info->resource,
		&fractional_scale_interface, info,
		fractional_scale_handle_resource_destroy);

	if (info->scale != 0) {
		wp_fractional_scale_v1_send_preferred_scale(info->resource, double_to_v120(info->scale));
	}
}

static void fractional_scale_manager_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wp_fractional_scale_manager_v1_interface scale_manager_interface = {
	.destroy = fractional_scale_manager_handle_destroy,
	.get_fractional_scale = handle_get_fractional_scale,
};

static void fractional_scale_manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_fractional_scale_manager_v1 *mgr = data;

	struct wl_resource *resource = wl_resource_create(client,
		&wp_fractional_scale_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &scale_manager_interface, mgr, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_fractional_scale_manager_v1 *mgr =
		wl_container_of(listener, mgr, display_destroy);
	wl_signal_emit_mutable(&mgr->events.destroy, NULL);
	assert(wl_list_empty(&mgr->events.destroy.listener_list));
	wl_list_remove(&mgr->display_destroy.link);
	free(mgr);
}

struct wlr_fractional_scale_manager_v1 *wlr_fractional_scale_manager_v1_create(
		struct wl_display *display, uint32_t version) {
	assert(version <= FRACTIONAL_SCALE_VERSION);

	struct wlr_fractional_scale_manager_v1 *mgr = calloc(1, sizeof(*mgr));
	if (mgr == NULL) {
		return NULL;
	}

	mgr->global = wl_global_create(display,
		&wp_fractional_scale_manager_v1_interface,
		version, mgr, fractional_scale_manager_bind);
	if (mgr->global == NULL) {
		free(mgr);
		return NULL;
	}

	mgr->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &mgr->display_destroy);

	wl_signal_init(&mgr->events.destroy);

	return mgr;
}

