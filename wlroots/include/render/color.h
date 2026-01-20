#ifndef RENDER_COLOR_H
#define RENDER_COLOR_H

#include <stdint.h>
#include <wlr/render/color.h>
#include <wlr/util/addon.h>

enum wlr_color_transform_type {
	COLOR_TRANSFORM_INVERSE_EOTF,
	COLOR_TRANSFORM_LCMS2,
	COLOR_TRANSFORM_LUT_3X1D,
	COLOR_TRANSFORM_MATRIX,
	COLOR_TRANSFORM_PIPELINE,
};

struct wlr_color_transform {
	int ref_count;
	struct wlr_addon_set addons; // per-renderer helper state

	enum wlr_color_transform_type type;
};

struct wlr_color_transform_inverse_eotf {
	struct wlr_color_transform base;

	enum wlr_color_transfer_function tf;
};

/**
 * The formula is approximated via three 1D look-up tables. The flat lut_3x1d
 * array has a length of 3 * dim.
 *
 * The offset of a color value for a given channel and color index is:
 *
 *     offset = channel_index * dim + color_index
 */
struct wlr_color_transform_lut_3x1d {
	struct wlr_color_transform base;

	uint16_t *lut_3x1d;
	size_t dim;
};

struct wlr_color_transform_matrix {
	struct wlr_color_transform base;

	float matrix[9];
};

struct wlr_color_transform_pipeline {
	struct wlr_color_transform base;

	struct wlr_color_transform **transforms;
	size_t len;
};

void wlr_color_transform_init(struct wlr_color_transform *tr,
	enum wlr_color_transform_type type);

/**
 * Get a struct wlr_color_transform_lcms2 from a generic struct wlr_color_transform.
 * Asserts that the base type is COLOR_TRANSFORM_LCMS2.
 */
struct wlr_color_transform_lcms2 *color_transform_lcms2_from_base(
	struct wlr_color_transform *tr);

void color_transform_lcms2_finish(struct wlr_color_transform_lcms2 *tr);

/**
 * Evaluate a LCMS2 color transform for a given RGB triplet.
 */
void color_transform_lcms2_eval(struct wlr_color_transform_lcms2 *tr,
	float out[static 3], const float in[static 3]);

/**
 * Gets a wlr_color_transform_inverse_eotf from a generic wlr_color_transform.
 * Asserts that the base type is COLOR_TRANSFORM_INVERSE_EOTF
 */
struct wlr_color_transform_inverse_eotf *wlr_color_transform_inverse_eotf_from_base(
	struct wlr_color_transform *tr);

/**
 * Get a struct wlr_color_transform_lut_3x1d from a generic
 * struct wlr_color_transform. Asserts that the base type is
 * COLOR_TRANSFORM_LUT_3X1D.
 */
struct wlr_color_transform_lut_3x1d *color_transform_lut_3x1d_from_base(
	struct wlr_color_transform *tr);

/**
 * Compute the matrix to convert RGB color values to CIE 1931 XYZ.
 */
void wlr_color_primaries_to_xyz(const struct wlr_color_primaries *primaries, float matrix[static 9]);

/**
 * Get default luminances for a transfer function.
 */
void wlr_color_transfer_function_get_default_luminance(enum wlr_color_transfer_function tf,
	struct wlr_color_luminances *lum);

#endif
