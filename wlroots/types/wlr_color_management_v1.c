#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/types/wlr_color_management_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/addon.h>

#include "color-management-v1-protocol.h"
#include "render/color.h"
#include "util/mem.h"

#define COLOR_MANAGEMENT_V1_VERSION 1

struct wlr_color_management_output_v1 {
	struct wl_resource *resource;
	struct wlr_output *output;
	struct wlr_color_manager_v1 *manager;
	struct wl_list link;

	struct wl_listener output_destroy;
};

struct wlr_color_management_surface_v1_state {
	bool has_image_desc_data;
	struct wlr_image_description_v1_data image_desc_data;
};

struct wlr_color_management_surface_v1 {
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wlr_color_manager_v1 *manager;

	struct wlr_addon addon;
	struct wlr_surface_synced synced;

	struct wlr_color_management_surface_v1_state current, pending;
};

struct wlr_color_management_surface_feedback_v1 {
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wlr_color_manager_v1 *manager;
	struct wl_list link;

	struct wlr_image_description_v1_data data;

	struct wl_listener surface_destroy;
};

struct wlr_image_description_v1 {
	struct wl_resource *resource;
	bool get_info_allowed;
	struct wlr_image_description_v1_data data; // immutable
};

struct wlr_image_description_creator_params_v1 {
	struct wl_resource *resource;
	struct wlr_color_manager_v1 *manager;
	struct wlr_image_description_v1_data data;
};

static void resource_handle_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static int32_t encode_cie1931_coord(float value) {
	return round(value * 1000 * 1000);
}

static float decode_cie1931_coord(int32_t raw) {
	return (float)raw / (1000 * 1000);
}

static const struct wp_image_description_v1_interface image_desc_impl;

static struct wlr_image_description_v1 *image_desc_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wp_image_description_v1_interface, &image_desc_impl));
	return wl_resource_get_user_data(resource);
}

static void image_desc_handle_get_information(struct wl_client *client,
		struct wl_resource *image_desc_resource, uint32_t id) {
	struct wlr_image_description_v1 *image_desc = image_desc_from_resource(image_desc_resource);
	if (image_desc == NULL) {
		wl_resource_post_error(image_desc_resource,
			WP_IMAGE_DESCRIPTION_V1_ERROR_NOT_READY,
			"image description is in failed state");
		return;
	}

	if (!image_desc->get_info_allowed) {
		wl_resource_post_error(image_desc_resource,
			WP_IMAGE_DESCRIPTION_V1_ERROR_NO_INFORMATION,
			"get_information not allowed");
		return;
	}

	uint32_t version = wl_resource_get_version(image_desc_resource);
	struct wl_resource *resource = wl_resource_create(client,
		&wp_image_description_info_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	struct wlr_color_primaries primaries;
	wlr_color_primaries_from_named(&primaries,
		wlr_color_manager_v1_primaries_to_wlr(image_desc->data.primaries_named));

	struct wlr_color_luminances luminances;
	wlr_color_transfer_function_get_default_luminance(
		wlr_color_manager_v1_transfer_function_to_wlr(image_desc->data.tf_named), &luminances);

	wp_image_description_info_v1_send_primaries_named(resource, image_desc->data.primaries_named);
	wp_image_description_info_v1_send_primaries(resource,
		encode_cie1931_coord(primaries.red.x), encode_cie1931_coord(primaries.red.y),
		encode_cie1931_coord(primaries.green.x), encode_cie1931_coord(primaries.green.y),
		encode_cie1931_coord(primaries.blue.x), encode_cie1931_coord(primaries.blue.y),
		encode_cie1931_coord(primaries.white.x), encode_cie1931_coord(primaries.white.y));
	wp_image_description_info_v1_send_tf_named(resource, image_desc->data.tf_named);
	wp_image_description_info_v1_send_luminances(resource,
		round(luminances.min * 10000), round(luminances.max),
		round(luminances.reference));
	// TODO: send mastering display primaries and luminances here when we add
	// support for features.set_mastering_display_primaries
	wp_image_description_info_v1_send_target_primaries(resource,
		encode_cie1931_coord(primaries.red.x), encode_cie1931_coord(primaries.red.y),
		encode_cie1931_coord(primaries.green.x), encode_cie1931_coord(primaries.green.y),
		encode_cie1931_coord(primaries.blue.x), encode_cie1931_coord(primaries.blue.y),
		encode_cie1931_coord(primaries.white.x), encode_cie1931_coord(primaries.white.y));
	wp_image_description_info_v1_send_target_luminance(resource,
		round(luminances.min * 10000), round(luminances.max));
	// TODO: send target_max_cll and target_max_fall
	wp_image_description_info_v1_send_done(resource);
	wl_resource_destroy(resource);
}

static const struct wp_image_description_v1_interface image_desc_impl = {
	.destroy = resource_handle_destroy,
	.get_information = image_desc_handle_get_information,
};

static void image_desc_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_image_description_v1 *image_desc = image_desc_from_resource(resource);
	free(image_desc);
}

static struct wl_resource *image_desc_create_resource(
		struct wl_resource *parent_resource, uint32_t id) {
	struct wl_client *client = wl_resource_get_client(parent_resource);
	uint32_t version = wl_resource_get_version(parent_resource);
	return wl_resource_create(client, &wp_image_description_v1_interface,
		version, id);
}

static void image_desc_create_ready(struct wlr_color_manager_v1 *manager,
		struct wl_resource *parent_resource, uint32_t id,
		const struct wlr_image_description_v1_data *data,
		bool get_info_allowed) {
	struct wlr_image_description_v1 *image_desc = calloc(1, sizeof(*image_desc));
	if (image_desc == NULL) {
		wl_resource_post_no_memory(parent_resource);
		return;
	}

	image_desc->data = *data;
	image_desc->get_info_allowed = get_info_allowed;

	image_desc->resource = image_desc_create_resource(parent_resource, id);
	if (!image_desc->resource) {
		wl_resource_post_no_memory(parent_resource);
		free(image_desc);
		return;
	}
	wl_resource_set_implementation(image_desc->resource, &image_desc_impl,
		image_desc, image_desc_handle_resource_destroy);

	// TODO: de-duplicate identity
	uint32_t identity = ++manager->last_image_desc_identity;
	wp_image_description_v1_send_ready(image_desc->resource, identity);
}

static void image_desc_create_failed(struct wl_resource *parent_resource, uint32_t id,
		enum wp_image_description_v1_cause cause, const char *msg) {
	struct wl_resource *resource = image_desc_create_resource(parent_resource, id);
	if (resource == NULL) {
		wl_resource_post_no_memory(parent_resource);
		return;
	}
	wl_resource_set_implementation(resource, &image_desc_impl, NULL, NULL);

	wp_image_description_v1_send_failed(resource, cause, msg);
}

static const struct wp_color_management_output_v1_interface cm_output_impl;

static struct wlr_color_management_output_v1 *cm_output_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wp_color_management_output_v1_interface, &cm_output_impl));
	return wl_resource_get_user_data(resource);
}

static void cm_output_handle_get_image_description(struct wl_client *client,
		struct wl_resource *cm_output_resource, uint32_t id) {
	struct wlr_color_management_output_v1 *cm_output = cm_output_from_resource(cm_output_resource);
	if (cm_output == NULL) {
		image_desc_create_failed(cm_output_resource, id,
			WP_IMAGE_DESCRIPTION_V1_CAUSE_NO_OUTPUT,
			"output has been destroyed");
		return;
	}

	struct wlr_image_description_v1_data data = {
		.tf_named = WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22,
		.primaries_named = WP_COLOR_MANAGER_V1_PRIMARIES_SRGB,
	};
	const struct wlr_output_image_description *image_desc = cm_output->output->image_description;
	if (image_desc != NULL) {
		data.tf_named = wlr_color_manager_v1_transfer_function_from_wlr(image_desc->transfer_function);
		data.primaries_named = wlr_color_manager_v1_primaries_from_wlr(image_desc->primaries);
	}
	image_desc_create_ready(cm_output->manager, cm_output_resource, id, &data, true);
}

static const struct wp_color_management_output_v1_interface cm_output_impl = {
	.destroy = resource_handle_destroy,
	.get_image_description = cm_output_handle_get_image_description,
};

static void cm_output_destroy(struct wlr_color_management_output_v1 *cm_output) {
	if (cm_output == NULL) {
		return;
	}
	wl_resource_set_user_data(cm_output->resource, NULL); // make inert
	wl_list_remove(&cm_output->output_destroy.link);
	wl_list_remove(&cm_output->link);
	free(cm_output);
}

static void cm_output_handle_output_destroy(struct wl_listener *listener, void *data) {
	struct wlr_color_management_output_v1 *cm_output = wl_container_of(listener, cm_output, output_destroy);
	cm_output_destroy(cm_output);
}

static void cm_output_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_color_management_output_v1 *cm_output = cm_output_from_resource(resource);
	cm_output_destroy(cm_output);
}

static void cm_surface_destroy(struct wlr_color_management_surface_v1 *cm_surface) {
	if (cm_surface == NULL) {
		return;
	}
	wl_resource_set_user_data(cm_surface->resource, NULL); // make inert
	wlr_surface_synced_finish(&cm_surface->synced);
	wlr_addon_finish(&cm_surface->addon);
	free(cm_surface);
}

static const struct wlr_surface_synced_impl cm_surface_synced_impl = {
	.state_size = sizeof(struct wlr_color_management_surface_v1_state),
};

static void cm_surface_handle_addon_destroy(struct wlr_addon *addon) {
	struct wlr_color_management_surface_v1 *cm_surface = wl_container_of(addon, cm_surface, addon);
	cm_surface_destroy(cm_surface);
}

static const struct wlr_addon_interface cm_surface_addon_impl = {
	.name = "wlr_color_management_surface_v1",
	.destroy = cm_surface_handle_addon_destroy,
};

static const struct wp_color_management_surface_v1_interface cm_surface_impl;

static struct wlr_color_management_surface_v1 *cm_surface_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wp_color_management_surface_v1_interface, &cm_surface_impl));
	return wl_resource_get_user_data(resource);
}

static void cm_surface_handle_set_image_description(struct wl_client *client,
		struct wl_resource *cm_surface_resource,
		struct wl_resource *image_desc_resource, uint32_t render_intent) {
	struct wlr_color_management_surface_v1 *cm_surface = cm_surface_from_resource(cm_surface_resource);
	if (cm_surface == NULL) {
		wl_resource_post_error(cm_surface_resource,
			WP_COLOR_MANAGEMENT_SURFACE_V1_ERROR_INERT,
			"set_image_description cannot be sent on an inert object");
		return;
	}

	struct wlr_image_description_v1 *image_desc = image_desc_from_resource(image_desc_resource);

	if (image_desc == NULL) {
		wl_resource_post_error(cm_surface_resource,
			WP_COLOR_MANAGEMENT_SURFACE_V1_ERROR_IMAGE_DESCRIPTION,
			"Image description to be set is invalid");
		return;
	}

	bool found = false;
	for (size_t i = 0; i < cm_surface->manager->render_intents_len; i++) {
		if (cm_surface->manager->render_intents[i] == render_intent) {
			found = true;
			break;
		}
	}
	if (!found) {
		wl_resource_post_error(cm_surface_resource,
			WP_COLOR_MANAGEMENT_SURFACE_V1_ERROR_RENDER_INTENT,
			"invalid render intent");
		return;
	}

	cm_surface->pending.has_image_desc_data = true;
	cm_surface->pending.image_desc_data = image_desc->data;
}

static void cm_surface_handle_unset_image_description(struct wl_client *client,
		struct wl_resource *cm_surface_resource) {
	struct wlr_color_management_surface_v1 *cm_surface = cm_surface_from_resource(cm_surface_resource);
	if (cm_surface == NULL) {
		wl_resource_post_error(cm_surface_resource,
			WP_COLOR_MANAGEMENT_SURFACE_V1_ERROR_INERT,
			"set_image_description cannot be sent on an inert object");
		return;
	}

	cm_surface->pending.has_image_desc_data = false;
}

static const struct wp_color_management_surface_v1_interface cm_surface_impl = {
	.destroy = resource_handle_destroy,
	.set_image_description = cm_surface_handle_set_image_description,
	.unset_image_description = cm_surface_handle_unset_image_description,
};

static void cm_surface_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_color_management_surface_v1 *cm_surface = cm_surface_from_resource(resource);
	cm_surface_destroy(cm_surface);
}

static const struct wp_color_management_surface_feedback_v1_interface surface_feedback_impl;

static struct wlr_color_management_surface_feedback_v1 *surface_feedback_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wp_color_management_surface_feedback_v1_interface, &surface_feedback_impl));
	return wl_resource_get_user_data(resource);
}

static void surface_feedback_handle_get_preferred(struct wl_client *client,
		struct wl_resource *surface_feedback_resource, uint32_t id) {
	struct wlr_color_management_surface_feedback_v1 *surface_feedback =
		surface_feedback_from_resource(surface_feedback_resource);
	if (surface_feedback == NULL) {
		wl_resource_post_error(surface_feedback_resource,
			WP_COLOR_MANAGEMENT_SURFACE_FEEDBACK_V1_ERROR_INERT,
			"get_preferred sent on inert feedback surface");
		return;
	}

	image_desc_create_ready(surface_feedback->manager,
		surface_feedback_resource, id, &surface_feedback->data, true);
}

static void surface_feedback_handle_get_preferred_parametric(struct wl_client *client,
		struct wl_resource *surface_feedback_resource, uint32_t id) {
	struct wlr_color_management_surface_feedback_v1 *surface_feedback =
		surface_feedback_from_resource(surface_feedback_resource);
	if (surface_feedback == NULL) {
		wl_resource_post_error(surface_feedback_resource,
			WP_COLOR_MANAGEMENT_SURFACE_FEEDBACK_V1_ERROR_INERT,
			"get_preferred_parametric sent on inert feedback surface");
		return;
	}

	image_desc_create_ready(surface_feedback->manager,
		surface_feedback_resource, id, &surface_feedback->data, true);
}

static const struct wp_color_management_surface_feedback_v1_interface surface_feedback_impl = {
	.destroy = resource_handle_destroy,
	.get_preferred = surface_feedback_handle_get_preferred,
	.get_preferred_parametric = surface_feedback_handle_get_preferred_parametric,
};

static void surface_feedback_destroy(struct wlr_color_management_surface_feedback_v1 *surface_feedback) {
	if (surface_feedback == NULL) {
		return;
	}
	wl_resource_set_user_data(surface_feedback->resource, NULL); // make inert
	wl_list_remove(&surface_feedback->surface_destroy.link);
	wl_list_remove(&surface_feedback->link);
	free(surface_feedback);
}

static void surface_feedback_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_color_management_surface_feedback_v1 *surface_feedback =
		surface_feedback_from_resource(resource);
	surface_feedback_destroy(surface_feedback);
}

static void surface_feedback_handle_surface_destroy(struct wl_listener *listener, void *data) {
	struct wlr_color_management_surface_feedback_v1 *surface_feedback =
		wl_container_of(listener, surface_feedback, surface_destroy);
	surface_feedback_destroy(surface_feedback);
}

static const struct wp_image_description_creator_params_v1_interface image_desc_creator_params_impl;

static struct wlr_image_description_creator_params_v1 *
image_desc_creator_params_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_image_description_creator_params_v1_interface,
		&image_desc_creator_params_impl));
	return wl_resource_get_user_data(resource);
}

static bool check_mastering_luminance_range(struct wl_resource *params_resource,
		const struct wlr_image_description_v1_data *data,
		float value, const char *name) {
	if (!data->has_mastering_luminance || value == 0) {
		return true;
	}

	if (value <= data->mastering_luminance.min) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE,
			"%s must be greater than min L of the mastering luminance range",
			name);
		return false;
	}
	if (value > data->mastering_luminance.max) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE,
			"%s must be less or equal to max L of the mastering luminance range",
			name);
		return false;
	}

	return true;
}

static void image_desc_creator_params_handle_create(struct wl_client *client,
		struct wl_resource *params_resource, uint32_t id) {
	struct wlr_image_description_creator_params_v1 *params =
		image_desc_creator_params_from_resource(params_resource);

	if (params->data.tf_named == 0) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INCOMPLETE_SET,
			"missing transfer function");
		return;
	}
	if (params->data.primaries_named == 0) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INCOMPLETE_SET,
			"missing primaries");
		return;
	}

	if (params->data.max_cll != 0 && params->data.max_fall != 0 &&
			params->data.max_fall > params->data.max_cll) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE,
			"max_fall must be less or equal to max_cll");
		return;
	}

	if (!check_mastering_luminance_range(params_resource, &params->data, params->data.max_cll, "max_cll") ||
			!check_mastering_luminance_range(params_resource, &params->data, params->data.max_fall, "max_fall")) {
		return;
	}

	// TODO: check that the target color volume is contained within the
	// primary color volume

	image_desc_create_ready(params->manager, params_resource, id, &params->data, false);
}

static void image_desc_creator_params_handle_set_tf_named(struct wl_client *client,
		struct wl_resource *params_resource, uint32_t tf) {
	struct wlr_image_description_creator_params_v1 *params =
		image_desc_creator_params_from_resource(params_resource);

	if (params->data.tf_named != 0) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET,
			"transfer function already set");
		return;
	}

	bool found = false;
	for (size_t i = 0; i < params->manager->transfer_functions_len; i++) {
		if (params->manager->transfer_functions[i] == tf) {
			found = true;
			break;
		}
	}
	if (!found) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_TF,
			"invalid transfer function");
		return;
	}

	params->data.tf_named = tf;
}

static void image_desc_creator_params_handle_set_tf_power(struct wl_client *client,
		struct wl_resource *params_resource, uint32_t eexp) {
	wl_resource_post_error(params_resource,
		WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_UNSUPPORTED_FEATURE,
		"set_tf_power is not supported");
}

static void image_desc_creator_params_handle_set_primaries_named(struct wl_client *client,
		struct wl_resource *params_resource, uint32_t primaries) {
	struct wlr_image_description_creator_params_v1 *params =
		image_desc_creator_params_from_resource(params_resource);

	if (params->data.primaries_named != 0) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET,
			"primaries already set");
		return;
	}

	bool found = false;
	for (size_t i = 0; i < params->manager->primaries_len; i++) {
		if (params->manager->primaries[i] == primaries) {
			found = true;
			break;
		}
	}
	if (!found) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_PRIMARIES_NAMED,
			"invalid primaries");
		return;
	}

	params->data.primaries_named = primaries;
}

static void image_desc_creator_params_handle_set_primaries(struct wl_client *client,
		struct wl_resource *params_resource, int32_t r_x, int32_t r_y,
		int32_t g_x, int32_t g_y, int32_t b_x, int32_t b_y,
		int32_t w_x, int32_t w_y) {
	wl_resource_post_error(params_resource,
		WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_UNSUPPORTED_FEATURE,
		"set_primaries is not supported");
}

static void image_desc_creator_params_handle_set_luminances(struct wl_client *client,
		struct wl_resource *params_resource, uint32_t min_lum,
		uint32_t max_lum, uint32_t reference_lum) {
	wl_resource_post_error(params_resource,
		WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_UNSUPPORTED_FEATURE,
		"set_luminances is not supported");
}

static void image_desc_creator_params_handle_set_mastering_display_primaries(
		struct wl_client *client, struct wl_resource *params_resource,
		int32_t r_x, int32_t r_y, int32_t g_x, int32_t g_y,
		int32_t b_x, int32_t b_y, int32_t w_x, int32_t w_y) {
	struct wlr_image_description_creator_params_v1 *params =
		image_desc_creator_params_from_resource(params_resource);
	if (!params->manager->features.set_mastering_display_primaries) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_UNSUPPORTED_FEATURE,
			"set_mastering_display_primaries is not supported");
		return;
	}

	if (params->data.has_mastering_display_primaries) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET,
			"mastering display primaries already set");
		return;
	}

	params->data.has_mastering_display_primaries = true;
	params->data.mastering_display_primaries = (struct wlr_color_primaries){
		.red = { decode_cie1931_coord(r_x), decode_cie1931_coord(r_y) },
		.green = { decode_cie1931_coord(g_x), decode_cie1931_coord(g_y) },
		.blue = { decode_cie1931_coord(b_x), decode_cie1931_coord(b_y) },
		.white = { decode_cie1931_coord(w_x), decode_cie1931_coord(w_y) },
	};
}

static void image_desc_creator_params_handle_set_mastering_luminance(struct wl_client *client,
		struct wl_resource *params_resource, uint32_t min_lum, uint32_t max_lum) {
	struct wlr_image_description_creator_params_v1 *params =
		image_desc_creator_params_from_resource(params_resource);
	if (!params->manager->features.set_mastering_display_primaries) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_UNSUPPORTED_FEATURE,
			"set_mastering_luminance is not supported");
		return;
	}

	if (params->data.has_mastering_luminance) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_ALREADY_SET,
			"mastering luminance already set");
		return;
	}

	params->data.has_mastering_luminance = true;
	params->data.mastering_luminance.min = (float)min_lum / 10000;
	params->data.mastering_luminance.max = max_lum;

	if (params->data.mastering_luminance.max <= params->data.mastering_luminance.min) {
		wl_resource_post_error(params_resource,
			WP_IMAGE_DESCRIPTION_CREATOR_PARAMS_V1_ERROR_INVALID_LUMINANCE,
			"max luminance must be greater than min luminance");
		return;
	}
}

static void image_desc_creator_params_handle_set_max_cll(struct wl_client *client,
		struct wl_resource *params_resource, uint32_t max_cll) {
	struct wlr_image_description_creator_params_v1 *params =
		image_desc_creator_params_from_resource(params_resource);
	params->data.max_cll = max_cll;
}

static void image_desc_creator_params_handle_set_max_fall(struct wl_client *client,
		struct wl_resource *params_resource, uint32_t max_fall) {
	struct wlr_image_description_creator_params_v1 *params =
		image_desc_creator_params_from_resource(params_resource);
	params->data.max_fall = max_fall;
}

static const struct wp_image_description_creator_params_v1_interface image_desc_creator_params_impl = {
	.create = image_desc_creator_params_handle_create,
	.set_tf_named = image_desc_creator_params_handle_set_tf_named,
	.set_tf_power = image_desc_creator_params_handle_set_tf_power,
	.set_primaries_named = image_desc_creator_params_handle_set_primaries_named,
	.set_primaries = image_desc_creator_params_handle_set_primaries,
	.set_luminances = image_desc_creator_params_handle_set_luminances,
	.set_mastering_display_primaries = image_desc_creator_params_handle_set_mastering_display_primaries,
	.set_mastering_luminance = image_desc_creator_params_handle_set_mastering_luminance,
	.set_max_cll = image_desc_creator_params_handle_set_max_cll,
	.set_max_fall = image_desc_creator_params_handle_set_max_fall,
};

static void image_desc_creator_params_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_image_description_creator_params_v1 *params =
		image_desc_creator_params_from_resource(resource);
	free(params);
}

static const struct wp_color_manager_v1_interface manager_impl;

static struct wlr_color_manager_v1 *manager_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wp_color_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

static void manager_handle_get_output(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id,
		struct wl_resource *output_resource) {
	struct wlr_color_manager_v1 *manager = manager_from_resource(manager_resource);
	struct wlr_output *output = wlr_output_from_resource(output_resource);

	uint32_t version = wl_resource_get_version(manager_resource);
	struct wl_resource *cm_output_resource = wl_resource_create(client,
		&wp_color_management_output_v1_interface, version, id);
	if (!cm_output_resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(cm_output_resource, &cm_output_impl,
		NULL, cm_output_handle_resource_destroy);

	if (output == NULL) {
		return; // leave the wp_color_management_output_v1 resource inert
	}

	struct wlr_color_management_output_v1 *cm_output = calloc(1, sizeof(*cm_output));
	if (cm_output == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	cm_output->resource = cm_output_resource;
	cm_output->manager = manager;
	cm_output->output = output;

	cm_output->output_destroy.notify = cm_output_handle_output_destroy;
	wl_signal_add(&output->events.destroy, &cm_output->output_destroy);

	wl_list_insert(&manager->outputs, &cm_output->link);
	wl_resource_set_user_data(cm_output->resource, cm_output);
}

static struct wlr_color_management_surface_v1 *cm_surface_from_surface(struct wlr_surface *surface) {
	struct wlr_addon *addon = wlr_addon_find(&surface->addons, NULL, &cm_surface_addon_impl);
	if (addon == NULL) {
		return NULL;
	}
	struct wlr_color_management_surface_v1 *cm_surface = wl_container_of(addon, cm_surface, addon);
	return cm_surface;
}

static void manager_handle_get_surface(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wlr_color_manager_v1 *manager = manager_from_resource(manager_resource);
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);

	if (cm_surface_from_surface(surface) != NULL) {
		wl_resource_post_error(manager_resource,
			WP_COLOR_MANAGER_V1_ERROR_SURFACE_EXISTS,
			"wp_color_management_surface_v1 already constructed for this surface");
		return;
	}

	struct wlr_color_management_surface_v1 *cm_surface = calloc(1, sizeof(*cm_surface));
	if (cm_surface == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	if (!wlr_surface_synced_init(&cm_surface->synced, surface, &cm_surface_synced_impl,
			&cm_surface->pending, &cm_surface->current)) {
		wl_client_post_no_memory(client);
		free(cm_surface);
		return;
	}

	uint32_t version = wl_resource_get_version(manager_resource);
	cm_surface->resource = wl_resource_create(client,
		&wp_color_management_surface_v1_interface, version, id);
	if (!cm_surface->resource) {
		wl_client_post_no_memory(client);
		wlr_surface_synced_finish(&cm_surface->synced);
		free(cm_surface);
		return;
	}
	wl_resource_set_implementation(cm_surface->resource, &cm_surface_impl, cm_surface, cm_surface_handle_resource_destroy);

	cm_surface->manager = manager;
	cm_surface->surface = surface;

	wlr_addon_init(&cm_surface->addon, &surface->addons, NULL, &cm_surface_addon_impl);
}

static void manager_handle_get_surface_feedback(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wlr_color_manager_v1 *manager = manager_from_resource(manager_resource);
	struct wlr_surface *surface = wlr_surface_from_resource(surface_resource);

	struct wlr_color_management_surface_feedback_v1 *surface_feedback =
		calloc(1, sizeof(*surface_feedback));
	if (surface_feedback == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	surface_feedback->manager = manager;

	uint32_t version = wl_resource_get_version(manager_resource);
	surface_feedback->resource = wl_resource_create(client,
		&wp_color_management_surface_feedback_v1_interface, version, id);
	if (!surface_feedback->resource) {
		wl_client_post_no_memory(client);
		free(surface_feedback);
		return;
	}
	wl_resource_set_implementation(surface_feedback->resource, &surface_feedback_impl,
		surface_feedback, surface_feedback_handle_resource_destroy);

	surface_feedback->surface = surface;
	surface_feedback->data = (struct wlr_image_description_v1_data){
		.tf_named = WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22,
		.primaries_named = WP_COLOR_MANAGER_V1_PRIMARIES_SRGB,
	};

	surface_feedback->surface_destroy.notify = surface_feedback_handle_surface_destroy;
	wl_signal_add(&surface->events.destroy, &surface_feedback->surface_destroy);

	wl_list_insert(&manager->surface_feedbacks, &surface_feedback->link);
}

static void manager_handle_create_icc_creator(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id) {
	wl_resource_post_error(manager_resource,
		WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE,
		"new_icc_creator is not supported");
}

static void manager_handle_create_parametric_creator(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id) {
	struct wlr_color_manager_v1 *manager = manager_from_resource(manager_resource);
	if (!manager->features.parametric) {
		wl_resource_post_error(manager_resource,
			WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE,
			"new_parametric_creator is not supported");
		return;
	}

	struct wlr_image_description_creator_params_v1 *params = calloc(1, sizeof(*params));
	if (params == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	params->manager = manager;

	uint32_t version = wl_resource_get_version(manager_resource);
	params->resource = wl_resource_create(client,
		&wp_image_description_creator_params_v1_interface, version, id);
	if (!params->resource) {
		wl_client_post_no_memory(client);
		free(params);
		return;
	}
	wl_resource_set_implementation(params->resource, &image_desc_creator_params_impl,
		params, image_desc_creator_params_handle_resource_destroy);
}

static void manager_handle_create_windows_scrgb(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id) {
	wl_resource_post_error(manager_resource,
		WP_COLOR_MANAGER_V1_ERROR_UNSUPPORTED_FEATURE,
		"get_windows_scrgb is not supported");
}

static const struct wp_color_manager_v1_interface manager_impl = {
	.destroy = resource_handle_destroy,
	.get_output = manager_handle_get_output,
	.get_surface = manager_handle_get_surface,
	.get_surface_feedback = manager_handle_get_surface_feedback,
	.create_icc_creator = manager_handle_create_icc_creator,
	.create_parametric_creator = manager_handle_create_parametric_creator,
	.create_windows_scrgb = manager_handle_create_windows_scrgb,
};

static void manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_color_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&wp_color_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, manager, NULL);

	const bool features[] = {
		[WP_COLOR_MANAGER_V1_FEATURE_ICC_V2_V4] = manager->features.icc_v2_v4,
		[WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC] = manager->features.parametric,
		[WP_COLOR_MANAGER_V1_FEATURE_SET_PRIMARIES] = manager->features.set_primaries,
		[WP_COLOR_MANAGER_V1_FEATURE_SET_TF_POWER] = manager->features.set_tf_power,
		[WP_COLOR_MANAGER_V1_FEATURE_SET_LUMINANCES] = manager->features.set_luminances,
		[WP_COLOR_MANAGER_V1_FEATURE_SET_MASTERING_DISPLAY_PRIMARIES] = manager->features.set_mastering_display_primaries,
		[WP_COLOR_MANAGER_V1_FEATURE_EXTENDED_TARGET_VOLUME] = manager->features.extended_target_volume,
		[WP_COLOR_MANAGER_V1_FEATURE_WINDOWS_SCRGB] = manager->features.windows_scrgb,
	};

	for (uint32_t i = 0; i < sizeof(features) / sizeof(features[0]); i++) {
		if (features[i]) {
			wp_color_manager_v1_send_supported_feature(resource, i);
		}
	}
	for (size_t i = 0; i < manager->render_intents_len; i++) {
		wp_color_manager_v1_send_supported_intent(resource,
			manager->render_intents[i]);
	}
	for (size_t i = 0; i < manager->transfer_functions_len; i++) {
		wp_color_manager_v1_send_supported_tf_named(resource,
			manager->transfer_functions[i]);
	}
	for (size_t i = 0; i < manager->primaries_len; i++) {
		wp_color_manager_v1_send_supported_primaries_named(resource,
			manager->primaries[i]);
	}

	wp_color_manager_v1_send_done(resource);
}

static void manager_handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_color_manager_v1 *manager = wl_container_of(listener, manager, display_destroy);
	wl_signal_emit_mutable(&manager->events.destroy, NULL);
	assert(wl_list_empty(&manager->events.destroy.listener_list));
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager->render_intents);
	free(manager->transfer_functions);
	free(manager->primaries);
	free(manager);
}

struct wlr_color_manager_v1 *wlr_color_manager_v1_create(struct wl_display *display,
		uint32_t version, const struct wlr_color_manager_v1_options *options) {
	assert(version <= COLOR_MANAGEMENT_V1_VERSION);

	bool has_perceptual_render_intent = false;
	for (size_t i = 0; i < options->render_intents_len; i++) {
		if (options->render_intents[i] == WP_COLOR_MANAGER_V1_RENDER_INTENT_PERCEPTUAL) {
			has_perceptual_render_intent = true;
		}
	}
	assert(has_perceptual_render_intent);

	// TODO: add support for all of these features
	assert(!options->features.icc_v2_v4);
	assert(!options->features.set_primaries);
	assert(!options->features.set_tf_power);
	assert(!options->features.set_luminances);
	assert(!options->features.extended_target_volume);
	assert(!options->features.windows_scrgb);

	struct wlr_color_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->features = options->features;

	bool ok =
		memdup(&manager->render_intents, options->render_intents, sizeof(options->render_intents[0]) * options->render_intents_len) &&
		memdup(&manager->transfer_functions, options->transfer_functions, sizeof(options->transfer_functions[0]) * options->transfer_functions_len) &&
		memdup(&manager->primaries, options->primaries, sizeof(options->primaries[0]) * options->primaries_len);
	if (!ok) {
		goto err_options;
	}

	manager->render_intents_len = options->render_intents_len;
	manager->transfer_functions_len = options->transfer_functions_len;
	manager->primaries_len = options->primaries_len;

	wl_signal_init(&manager->events.destroy);
	wl_list_init(&manager->outputs);
	wl_list_init(&manager->surface_feedbacks);

	manager->global = wl_global_create(display, &wp_color_manager_v1_interface,
		version, manager, manager_bind);
	if (manager->global == NULL) {
		goto err_options;
	}

	manager->display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;

err_options:
	free(manager->render_intents);
	free(manager->transfer_functions);
	free(manager->primaries);
	free(manager);
	return NULL;
}

const struct wlr_image_description_v1_data *
wlr_surface_get_image_description_v1_data(struct wlr_surface *surface) {
	struct wlr_color_management_surface_v1 *cm_surface = cm_surface_from_surface(surface);
	if (cm_surface == NULL || !cm_surface->current.has_image_desc_data) {
		return NULL;
	}
	return &cm_surface->current.image_desc_data;
}

void wlr_color_manager_v1_set_surface_preferred_image_description(
		struct wlr_color_manager_v1 *manager, struct wlr_surface *surface,
		const struct wlr_image_description_v1_data *data) {
	// TODO: de-duplicate identity
	uint32_t identity = ++manager->last_image_desc_identity;

	struct wlr_color_management_surface_feedback_v1 *surface_feedback;
	wl_list_for_each(surface_feedback, &manager->surface_feedbacks, link) {
		if (surface_feedback->surface == surface) {
			surface_feedback->data = *data;
			wp_color_management_surface_feedback_v1_send_preferred_changed(
				surface_feedback->resource, identity);
		}
	}
}

enum wlr_color_transfer_function
wlr_color_manager_v1_transfer_function_to_wlr(enum wp_color_manager_v1_transfer_function tf) {
	switch (tf) {
	case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB:
		return WLR_COLOR_TRANSFER_FUNCTION_SRGB;
	case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ:
		return WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ;
	case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR:
		return WLR_COLOR_TRANSFER_FUNCTION_EXT_LINEAR;
	case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22:
		return WLR_COLOR_TRANSFER_FUNCTION_GAMMA22;
	case WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886:
		return WLR_COLOR_TRANSFER_FUNCTION_BT1886;
	default:
		abort();
	}
}

enum wp_color_manager_v1_transfer_function
wlr_color_manager_v1_transfer_function_from_wlr(enum wlr_color_transfer_function tf) {
	switch (tf) {
	case WLR_COLOR_TRANSFER_FUNCTION_SRGB:
		return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_SRGB;
	case WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ:
		return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
	case WLR_COLOR_TRANSFER_FUNCTION_EXT_LINEAR:
		return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
	case WLR_COLOR_TRANSFER_FUNCTION_GAMMA22:
		return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_GAMMA22;
	case WLR_COLOR_TRANSFER_FUNCTION_BT1886:
		return WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_BT1886;
	}
	abort();
}

enum wlr_color_named_primaries
wlr_color_manager_v1_primaries_to_wlr(enum wp_color_manager_v1_primaries primaries) {
	switch (primaries) {
	case WP_COLOR_MANAGER_V1_PRIMARIES_SRGB:
		return WLR_COLOR_NAMED_PRIMARIES_SRGB;
	case WP_COLOR_MANAGER_V1_PRIMARIES_BT2020:
		return WLR_COLOR_NAMED_PRIMARIES_BT2020;
	default:
		abort();
	}
}

enum wp_color_manager_v1_primaries
wlr_color_manager_v1_primaries_from_wlr(enum wlr_color_named_primaries primaries) {
	switch (primaries) {
	case WLR_COLOR_NAMED_PRIMARIES_SRGB:
		return WP_COLOR_MANAGER_V1_PRIMARIES_SRGB;
	case WLR_COLOR_NAMED_PRIMARIES_BT2020:
		return WP_COLOR_MANAGER_V1_PRIMARIES_BT2020;
	}
	abort();
}
