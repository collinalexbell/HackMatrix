#include <assert.h>
#include <stdlib.h>

#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_color_representation_v1.h>
#include <wlr/util/addon.h>
#include <wlr/util/log.h>

#include "color-representation-v1-protocol.h"
#include "util/mem.h"

#define WP_COLOR_REPRESENTATION_VERSION 1

enum wlr_alpha_mode wlr_color_representation_v1_alpha_mode_to_wlr(
		enum wp_color_representation_surface_v1_alpha_mode wp_val) {
	switch (wp_val) {
	case WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL:
		return WLR_COLOR_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_OPTICAL:
		return WLR_COLOR_ALPHA_MODE_PREMULTIPLIED_OPTICAL;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_STRAIGHT:
		return WLR_COLOR_ALPHA_MODE_STRAIGHT;
	}
	abort(); // unreachable
}

enum wlr_color_encoding wlr_color_representation_v1_color_encoding_to_wlr(
		enum wp_color_representation_surface_v1_coefficients wp_val) {
	switch (wp_val) {
	case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_IDENTITY:
		return WLR_COLOR_ENCODING_IDENTITY;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709:
		return WLR_COLOR_ENCODING_BT709;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_FCC:
		return WLR_COLOR_ENCODING_FCC;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601:
		return WLR_COLOR_ENCODING_BT601;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_SMPTE240:
		return WLR_COLOR_ENCODING_SMPTE240;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020:
		return WLR_COLOR_ENCODING_BT2020;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020_CL:
		return WLR_COLOR_ENCODING_BT2020_CL;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_ICTCP:
		return WLR_COLOR_ENCODING_ICTCP;
	}
	abort(); // unreachable
}

enum wlr_color_range wlr_color_representation_v1_color_range_to_wlr(
		enum wp_color_representation_surface_v1_range wp_val) {
	switch (wp_val) {
	case WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED:
		return  WLR_COLOR_RANGE_LIMITED;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL:
		return WLR_COLOR_RANGE_FULL;
	}
	abort(); // unreachable
}

enum wlr_color_chroma_location wlr_color_representation_v1_chroma_location_to_wlr(
		enum wp_color_representation_surface_v1_chroma_location wp_val) {
	switch (wp_val) {
	case WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_0:
		return WLR_COLOR_CHROMA_LOCATION_TYPE0;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_1:
		return WLR_COLOR_CHROMA_LOCATION_TYPE1;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_2:
		return WLR_COLOR_CHROMA_LOCATION_TYPE2;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_3:
		return WLR_COLOR_CHROMA_LOCATION_TYPE3;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_4:
		return WLR_COLOR_CHROMA_LOCATION_TYPE4;
	case WP_COLOR_REPRESENTATION_SURFACE_V1_CHROMA_LOCATION_TYPE_5:
		return WLR_COLOR_CHROMA_LOCATION_TYPE5;
	}
	abort(); // unreachable
}

struct wlr_color_representation_v1 {
	struct wl_resource *resource;
	struct wlr_surface *surface;

	struct wlr_color_representation_manager_v1 *manager;

	// Associate the wlr_color_representation_v1 with a wlr_surface
	struct wlr_addon addon;

	struct wlr_surface_synced synced;
	struct wlr_color_representation_v1_surface_state pending, current;
};

static const struct wp_color_representation_surface_v1_interface color_repr_impl;

static struct wlr_color_representation_v1 *color_repr_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_color_representation_surface_v1_interface,
		&color_repr_impl));
	return wl_resource_get_user_data(resource);
}

static void color_repr_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	// Actual destroying is done by the resource-destroy handler
	wl_resource_destroy(resource);
}

static void color_repr_handle_set_alpha_mode(struct wl_client *client,
		struct wl_resource *resource, uint32_t alpha_mode) {
	struct wlr_color_representation_v1 *color_repr =
		color_repr_from_resource(resource);
	if (color_repr == NULL) {
		wl_resource_post_error(resource, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT,
			"Associated surface has been destroyed, object is inert");
		return;
	}

	bool found = false;
	for (size_t i = 0; i < color_repr->manager->supported_alpha_modes_len; i++) {
		if (color_repr->manager->supported_alpha_modes[i] == alpha_mode) {
			found = true;
			break;
		}
	}
	if (!found) {
		wl_resource_post_error(resource,
			WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_ALPHA_MODE,
			"Unsupported alpha mode");
		return;
	}

	color_repr->pending.alpha_mode = alpha_mode;
}

static void color_repr_handle_set_coefficients_and_range(struct wl_client *client,
		struct wl_resource *resource, uint32_t coefficients,
		uint32_t range) {
	struct wlr_color_representation_v1 *color_repr =
		color_repr_from_resource(resource);
	if (color_repr == NULL) {
		wl_resource_post_error(resource, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT,
			"Associated surface has been destroyed, object is inert");
		return;
	}

	bool found = false;
	for (size_t i = 0; i < color_repr->manager->supported_coeffs_and_ranges_len; i++) {
		struct wlr_color_representation_v1_coeffs_and_range *supported =
			&color_repr->manager->supported_coeffs_and_ranges[i];
		if (supported->coeffs == coefficients && supported->range == range) {
			found = true;
			break;
		}
	}
	if (!found) {
		wl_resource_post_error(resource,
			WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_COEFFICIENTS,
			"Unsupported coefficients/range pair");
		return;
	}

	color_repr->pending.coefficients = coefficients;
	color_repr->pending.range = range;
}

static void color_repr_handle_set_chroma_location(struct wl_client *client,
		struct wl_resource *resource, uint32_t chroma_location) {
	struct wlr_color_representation_v1 *color_repr =
		color_repr_from_resource(resource);
	if (color_repr == NULL) {
		wl_resource_post_error(resource, WP_COLOR_REPRESENTATION_SURFACE_V1_ERROR_INERT,
			"Associated surface has been destroyed, object is inert");
		return;
	}

	uint32_t version = wl_resource_get_version(resource);
	if (!wp_color_representation_surface_v1_chroma_location_is_valid(
			version, chroma_location)) {
		wlr_log(WLR_ERROR, "Client sent chroma location which isn't a valid enum value");
		// TODO: Post actual error once
		// https://gitlab.freedesktop.org/wayland/wayland-protocols/-/merge_requests/429
		// is merged and wlroots depends on a new enough wayland-protocols.
		wl_client_post_implementation_error(resource->client,
			"Chroma location is not a valid enum value");
		return;
	}

	// In this protocol there's no concept of supported chroma locations
	// from a client point-of-view. The compositor should just ignore any
	// chroma locations it doesn't know what to do with.

	color_repr->pending.chroma_location = chroma_location;
}

static const struct wp_color_representation_surface_v1_interface color_repr_impl = {
	.destroy = color_repr_handle_destroy,
	.set_alpha_mode = color_repr_handle_set_alpha_mode,
	.set_coefficients_and_range = color_repr_handle_set_coefficients_and_range,
	.set_chroma_location = color_repr_handle_set_chroma_location,
};

static void color_repr_destroy(struct wlr_color_representation_v1 *color_repr) {
	if (color_repr == NULL) {
		return;
	}
	wlr_surface_synced_finish(&color_repr->synced);
	wlr_addon_finish(&color_repr->addon);
	wl_resource_set_user_data(color_repr->resource, NULL);
	free(color_repr);
}

static void color_repr_addon_destroy(struct wlr_addon *addon) {
	struct wlr_color_representation_v1 *color_repr =
		wl_container_of(addon, color_repr, addon);
	color_repr_destroy(color_repr);
}

static const struct wlr_addon_interface surface_addon_impl = {
	.name = "wlr_color_representation_v1",
	.destroy = color_repr_addon_destroy,
};

static void color_repr_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_color_representation_v1 *color_repr =
		color_repr_from_resource(resource);
	color_repr_destroy(color_repr);
}

static void color_repr_manager_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	// Actual destroying is done by the resource-destroy handler
	wl_resource_destroy(resource);
}

static const struct wlr_surface_synced_impl surface_synced_impl = {
	.state_size = sizeof(struct wlr_color_representation_v1_surface_state),
};

static struct wlr_color_representation_v1 *color_repr_from_surface(
		struct wlr_surface *surface) {
	struct wlr_addon *addon = wlr_addon_find(&surface->addons, NULL, &surface_addon_impl);
	if (addon == NULL) {
		return NULL;
	}
	struct wlr_color_representation_v1 *color_repr = wl_container_of(addon, color_repr, addon);
	return color_repr;
}

static const struct wp_color_representation_manager_v1_interface color_repr_manager_impl;

static struct wlr_color_representation_manager_v1 *manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_color_representation_manager_v1_interface,
		&color_repr_manager_impl));
	return wl_resource_get_user_data(resource);
}

static void color_repr_manager_handle_get_surface(struct wl_client *client,
		struct wl_resource *manager_resource,
		uint32_t color_repr_id,
		struct wl_resource *surface_resource) {
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);

	// Check if there's already a color-representation attached to
	// this surface
	if (color_repr_from_surface(surface) != NULL) {
		wl_resource_post_error(manager_resource,
			WP_COLOR_REPRESENTATION_MANAGER_V1_ERROR_SURFACE_EXISTS,
			"wp_color_representation_surface_v1 already exists for this surface");
		return;
	}

	struct wlr_color_representation_v1 *color_repr = calloc(1, sizeof(*color_repr));
	if (!color_repr) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	color_repr->manager = manager_from_resource(manager_resource);

	if (!wlr_surface_synced_init(&color_repr->synced, surface,
			&surface_synced_impl, &color_repr->pending, &color_repr->current)) {
		free(color_repr);
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	uint32_t version = wl_resource_get_version(manager_resource);
	color_repr->resource = wl_resource_create(client,
		&wp_color_representation_surface_v1_interface, version, color_repr_id);
	if (color_repr->resource == NULL) {
		wlr_surface_synced_finish(&color_repr->synced);
		free(color_repr);
		wl_resource_post_no_memory(manager_resource);
		return;
	}
	wl_resource_set_implementation(color_repr->resource,
		&color_repr_impl, color_repr, color_repr_handle_resource_destroy);

	wlr_addon_init(&color_repr->addon, &surface->addons, NULL, &surface_addon_impl);
}

static const struct wp_color_representation_manager_v1_interface color_repr_manager_impl = {
	.destroy = color_repr_manager_handle_destroy,
	.get_surface = color_repr_manager_handle_get_surface,
};

static void send_supported(struct wlr_color_representation_manager_v1 *manager,
		struct wl_resource *resource) {
	for (size_t i = 0; i < manager->supported_alpha_modes_len; i++) {
		wp_color_representation_manager_v1_send_supported_alpha_mode(
			resource, manager->supported_alpha_modes[i]);
	}

	for (size_t i = 0; i < manager->supported_coeffs_and_ranges_len; i++) {
		struct wlr_color_representation_v1_coeffs_and_range *supported =
			&manager->supported_coeffs_and_ranges[i];
		wp_color_representation_manager_v1_send_supported_coefficients_and_ranges(
			resource, supported->coeffs, supported->range);
	}

	// Note that there is no event for supported chroma locations in the
	// v1 protocol.

	wp_color_representation_manager_v1_send_done(resource);
}

static void manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_color_representation_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&wp_color_representation_manager_v1_interface,
		version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &color_repr_manager_impl, manager, NULL);

	send_supported(manager, resource);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_color_representation_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);

	wl_signal_emit_mutable(&manager->events.destroy, NULL);

	assert(wl_list_empty(&manager->events.destroy.listener_list));

	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager->supported_alpha_modes);
	free(manager->supported_coeffs_and_ranges);
	free(manager);
}

struct wlr_color_representation_manager_v1 *wlr_color_representation_manager_v1_create(
		struct wl_display *display, uint32_t version,
		const struct wlr_color_representation_v1_options *options) {
	assert(version <= WP_COLOR_REPRESENTATION_VERSION);

	struct wlr_color_representation_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	bool ok = true;
	ok &= memdup(&manager->supported_alpha_modes,
		options->supported_alpha_modes,
		sizeof(options->supported_alpha_modes[0]) * options->supported_alpha_modes_len);
	manager->supported_alpha_modes_len = options->supported_alpha_modes_len;
	ok &= memdup(&manager->supported_coeffs_and_ranges,
		options->supported_coeffs_and_ranges,
		sizeof(options->supported_coeffs_and_ranges[0]) * options->supported_coeffs_and_ranges_len);
	manager->supported_coeffs_and_ranges_len = options->supported_coeffs_and_ranges_len;
	if (!ok) {
		goto err_options;
	}

	manager->global = wl_global_create(display,
		&wp_color_representation_manager_v1_interface,
		version, manager, manager_bind);
	if (manager->global == NULL) {
		goto err_options;
	}

	wl_signal_init(&manager->events.destroy);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;

err_options:
	free(manager->supported_alpha_modes);
	free(manager->supported_coeffs_and_ranges);
	free(manager);
	return NULL;
}

static const enum wp_color_representation_surface_v1_coefficients coefficients[] = {
	WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_IDENTITY,
	WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT709,
	WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_FCC,
	WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT601,
	WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_SMPTE240,
	WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020,
	WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_BT2020_CL,
	WP_COLOR_REPRESENTATION_SURFACE_V1_COEFFICIENTS_ICTCP,
};

#define COEFFICIENTS_LEN (sizeof(coefficients) / sizeof(coefficients[0]))

static const enum wp_color_representation_surface_v1_range ranges[] = {
	WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_LIMITED,
	WP_COLOR_REPRESENTATION_SURFACE_V1_RANGE_FULL,
};

#define RANGES_LEN (sizeof(ranges) / sizeof(ranges[0]))

struct wlr_color_representation_manager_v1 *wlr_color_representation_manager_v1_create_with_renderer(
		struct wl_display *display, uint32_t version, struct wlr_renderer *renderer) {
	const enum wp_color_representation_surface_v1_alpha_mode alpha_modes[] = {
		WP_COLOR_REPRESENTATION_SURFACE_V1_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL,
	};

	struct wlr_color_representation_v1_coeffs_and_range coeffs_and_ranges[COEFFICIENTS_LEN * RANGES_LEN];
	size_t coeffs_and_ranges_len = 0;
	for (size_t i = 0; i < COEFFICIENTS_LEN; i++) {
		enum wp_color_representation_surface_v1_coefficients coeffs = coefficients[i];
		enum wlr_color_encoding enc = wlr_color_representation_v1_color_encoding_to_wlr(coeffs);
		if (!(renderer->color_encodings & enc)) {
			continue;
		}
		for (size_t j = 0; j < RANGES_LEN; j++) {
			coeffs_and_ranges[coeffs_and_ranges_len] = (struct wlr_color_representation_v1_coeffs_and_range){
				.coeffs = coeffs,
				.range = ranges[j],
			};
			coeffs_and_ranges_len++;
		}
	}

	const struct wlr_color_representation_v1_options options = {
		.supported_alpha_modes = alpha_modes,
		.supported_alpha_modes_len = sizeof(alpha_modes) / sizeof(alpha_modes[0]),
		.supported_coeffs_and_ranges = coeffs_and_ranges,
		.supported_coeffs_and_ranges_len = coeffs_and_ranges_len,
	};
	return wlr_color_representation_manager_v1_create(display, version, &options);
}

const struct wlr_color_representation_v1_surface_state *wlr_color_representation_v1_get_surface_state(
		struct wlr_surface *surface) {
	struct wlr_color_representation_v1 *color_repr = color_repr_from_surface(surface);
	if (color_repr == NULL) {
		return NULL;
	}
	return &color_repr->current;
}
