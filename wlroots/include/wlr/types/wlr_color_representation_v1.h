/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_COLOR_REPRESENTATION_V1_H
#define WLR_TYPES_WLR_COLOR_REPRESENTATION_V1_H

#include <wayland-server-core.h>
#include <wayland-protocols/color-representation-v1-enum.h>
#include <wlr/render/color.h>

struct wlr_surface;
struct wlr_renderer;

// Supported coefficients and range are always paired together
struct wlr_color_representation_v1_coeffs_and_range {
	enum wp_color_representation_surface_v1_coefficients coeffs;
	enum wp_color_representation_surface_v1_range range;
};

struct wlr_color_representation_manager_v1 {
	struct wl_global *global;

	struct {
		// Manager is being destroyed
		struct wl_signal destroy;
	} events;

	struct {
		enum wp_color_representation_surface_v1_alpha_mode
			*supported_alpha_modes;
		size_t supported_alpha_modes_len;

		struct wlr_color_representation_v1_coeffs_and_range
			*supported_coeffs_and_ranges;
		size_t supported_coeffs_and_ranges_len;

		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

// Options used when initialising a wlr_color_representation_manager_v1
struct wlr_color_representation_v1_options {
	const enum wp_color_representation_surface_v1_alpha_mode *supported_alpha_modes;
	size_t supported_alpha_modes_len;

	const struct wlr_color_representation_v1_coeffs_and_range *supported_coeffs_and_ranges;
	size_t supported_coeffs_and_ranges_len;
};

struct wlr_color_representation_manager_v1 *wlr_color_representation_manager_v1_create(
		struct wl_display *display, uint32_t version,
		const struct wlr_color_representation_v1_options *options);

struct wlr_color_representation_manager_v1 *wlr_color_representation_manager_v1_create_with_renderer(
		struct wl_display *display, uint32_t version, struct wlr_renderer *renderer);

// This is all the color-representation state which can be attached to a
// surface, double-buffered and made current on commit
struct wlr_color_representation_v1_surface_state {
	// The enum premultiplied_electrical has value zero and is defined
	// to be the default if unspecified.
	enum wp_color_representation_surface_v1_alpha_mode alpha_mode;

	// If zero then indicates unset, otherwise values correspond to
	// enum wp_color_representation_surface_v1_coefficients
	uint32_t coefficients;

	// If zero then indicates unset, otherwise values correspond to
	// enum wp_color_representation_surface_v1_range
	uint32_t range;

	// If zero then indicates unset, otherwise values correspond to
	// enum wp_color_representation_surface_v1_chroma_location
	uint32_t chroma_location;
};

// Get the current color representation state committed to a surface
const struct wlr_color_representation_v1_surface_state *wlr_color_representation_v1_get_surface_state(
	struct wlr_surface *surface);

enum wlr_alpha_mode wlr_color_representation_v1_alpha_mode_to_wlr(
	enum wp_color_representation_surface_v1_alpha_mode wp_val);
enum wlr_color_encoding wlr_color_representation_v1_color_encoding_to_wlr(
	enum wp_color_representation_surface_v1_coefficients wp_val);
enum wlr_color_range wlr_color_representation_v1_color_range_to_wlr(
	enum wp_color_representation_surface_v1_range wp_val);
enum wlr_color_chroma_location wlr_color_representation_v1_chroma_location_to_wlr(
	enum wp_color_representation_surface_v1_chroma_location wp_val);

#endif // WLR_TYPES_WLR_COLOR_REPRESENTATION_V1_H
