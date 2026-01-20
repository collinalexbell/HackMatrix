/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_COLOR_MANAGEMENT_V1_H
#define WLR_TYPES_WLR_COLOR_MANAGEMENT_V1_H

#include <wayland-server-core.h>
#include <wayland-protocols/color-management-v1-enum.h>

#include <wlr/render/color.h>

struct wlr_surface;

struct wlr_image_description_v1_data {
	uint32_t tf_named; // enum wp_color_manager_v1_transfer_function, zero if unset
	uint32_t primaries_named; // enum wp_color_manager_v1_primaries, zero if unset

	bool has_mastering_display_primaries;
	struct wlr_color_primaries mastering_display_primaries;

	bool has_mastering_luminance;
	struct {
		float min, max; // cd/m²
	} mastering_luminance;

	uint32_t max_cll, max_fall; // cd/m², zero if unset
};

struct wlr_color_manager_v1_features {
	bool icc_v2_v4;
	bool parametric;
	bool set_primaries;
	bool set_tf_power;
	bool set_luminances;
	bool set_mastering_display_primaries;
	bool extended_target_volume;
	bool windows_scrgb;
};

struct wlr_color_manager_v1_options {
	struct wlr_color_manager_v1_features features;

	const enum wp_color_manager_v1_render_intent *render_intents;
	size_t render_intents_len;

	const enum wp_color_manager_v1_transfer_function *transfer_functions;
	size_t transfer_functions_len;

	const enum wp_color_manager_v1_primaries *primaries;
	size_t primaries_len;
};

struct wlr_color_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	struct {
		struct wlr_color_manager_v1_features features;

		enum wp_color_manager_v1_render_intent *render_intents;
		size_t render_intents_len;

		enum wp_color_manager_v1_transfer_function *transfer_functions;
		size_t transfer_functions_len;

		enum wp_color_manager_v1_primaries *primaries;
		size_t primaries_len;

		struct wl_list outputs; // wlr_color_management_output_v1.link
		struct wl_list surface_feedbacks; // wlr_color_management_surface_feedback_v1.link

		uint32_t last_image_desc_identity;

		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_color_manager_v1 *wlr_color_manager_v1_create(struct wl_display *display,
	uint32_t version, const struct wlr_color_manager_v1_options *options);

const struct wlr_image_description_v1_data *
wlr_surface_get_image_description_v1_data(struct wlr_surface *surface);

void wlr_color_manager_v1_set_surface_preferred_image_description(
	struct wlr_color_manager_v1 *manager, struct wlr_surface *surface,
	const struct wlr_image_description_v1_data *data);

/**
 * Convert a protocol transfer function to enum wlr_color_transfer_function.
 * Aborts if there is no matching wlroots entry.
 */
enum wlr_color_transfer_function
wlr_color_manager_v1_transfer_function_to_wlr(enum wp_color_manager_v1_transfer_function tf);

/**
 * Convert an enum wlr_color_transfer_function value into a protocol transfer function.
 */
enum wp_color_manager_v1_transfer_function
wlr_color_manager_v1_transfer_function_from_wlr(enum wlr_color_transfer_function tf);

/**
 * Convert a protocol named primaries to enum wlr_color_named_primaries.
 * Aborts if there is no matching wlroots entry.
 */
enum wlr_color_named_primaries
wlr_color_manager_v1_primaries_to_wlr(enum wp_color_manager_v1_primaries primaries);

/**
 * Convert an enum wlr_color_named_primaries value into protocol primaries.
 */
enum wp_color_manager_v1_primaries
wlr_color_manager_v1_primaries_from_wlr(enum wlr_color_named_primaries primaries);

#endif
