/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_RENDER_COLOR_H
#define WLR_RENDER_COLOR_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

/**
 * Well-known color primaries.
 */
enum wlr_color_named_primaries {
	WLR_COLOR_NAMED_PRIMARIES_SRGB = 1 << 0,
	WLR_COLOR_NAMED_PRIMARIES_BT2020 = 1 << 1,
};

/**
 * Well-known color transfer functions.
 */
enum wlr_color_transfer_function {
	WLR_COLOR_TRANSFER_FUNCTION_SRGB = 1 << 0,
	WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ = 1 << 1,
	WLR_COLOR_TRANSFER_FUNCTION_EXT_LINEAR = 1 << 2,
	WLR_COLOR_TRANSFER_FUNCTION_GAMMA22 = 1 << 3,
	WLR_COLOR_TRANSFER_FUNCTION_BT1886 = 1 << 4,
};

/**
 * Specifies alpha blending modes.  Note that premultiplied_electrical
 * is the default, so there is no "none" or "unset" value.
 */
enum wlr_alpha_mode {
	WLR_COLOR_ALPHA_MODE_PREMULTIPLIED_ELECTRICAL,
	WLR_COLOR_ALPHA_MODE_PREMULTIPLIED_OPTICAL,
	WLR_COLOR_ALPHA_MODE_STRAIGHT,
};

/**
 * Well-known color encodings, each representing a set of matrix coefficients
 * used to convert that particular YCbCr encoding to RGB.  NONE means the
 * value is unset or unknown.
 */
enum wlr_color_encoding {
	WLR_COLOR_ENCODING_NONE = 0,
	WLR_COLOR_ENCODING_IDENTITY = 1 << 0,
	WLR_COLOR_ENCODING_BT709 = 1 << 1,
	WLR_COLOR_ENCODING_FCC = 1 << 2,
	WLR_COLOR_ENCODING_BT601 = 1 << 3,
	WLR_COLOR_ENCODING_SMPTE240 = 1 << 4,
	WLR_COLOR_ENCODING_BT2020 = 1 << 5,
	WLR_COLOR_ENCODING_BT2020_CL = 1 << 6,
	WLR_COLOR_ENCODING_ICTCP = 1 << 7,
};

/**
 * Specifies whether a particular color-encoding uses full- or limited-range
 * values.  NONE means the value is unset or unknown.
 */
enum wlr_color_range {
	WLR_COLOR_RANGE_NONE,
	WLR_COLOR_RANGE_LIMITED,
	WLR_COLOR_RANGE_FULL,
};

/**
 * Chroma sample locations, corresponding to Chroma420SampleLocType code
 * points in H.273.  NONE means the value is unset or unknown.
 */
enum wlr_color_chroma_location {
	WLR_COLOR_CHROMA_LOCATION_NONE,
	WLR_COLOR_CHROMA_LOCATION_TYPE0,
	WLR_COLOR_CHROMA_LOCATION_TYPE1,
	WLR_COLOR_CHROMA_LOCATION_TYPE2,
	WLR_COLOR_CHROMA_LOCATION_TYPE3,
	WLR_COLOR_CHROMA_LOCATION_TYPE4,
	WLR_COLOR_CHROMA_LOCATION_TYPE5,
};

/**
 * CIE 1931 xy chromaticity coordinates.
 */
struct wlr_color_cie1931_xy {
	float x, y;
};

/**
 * Color primaries and white point describing a color volume.
 */
struct wlr_color_primaries {
	struct wlr_color_cie1931_xy red, green, blue, white;
};

/**
 * Luminance range and reference white luminance level, in cd/m².
 */
struct wlr_color_luminances {
	float min, max, reference;
};

/**
 * A color transformation formula, which maps a linear color space with
 * sRGB primaries to an output color space.
 *
 * For ease of use, this type is heap allocated and reference counted.
 * Use wlr_color_transform_ref()/wlr_color_transform_unref(). The initial reference
 * count after creation is 1.
 *
 * Color transforms are immutable; their type/parameters should not be changed,
 * and this API provides no functions to modify them after creation.
 *
 * This formula may be implemented using a 3d look-up table, or some other
 * means.
 */
struct wlr_color_transform;

/**
 * Initialize a color transformation to convert linear
 * (with sRGB(?) primaries) to an ICC profile. Returns NULL on failure.
 */
struct wlr_color_transform *wlr_color_transform_init_linear_to_icc(
	const void *data, size_t size);

/**
 * Initialize a color transformation to apply EOTF⁻¹ encoding. Returns
 * NULL on failure.
 */
struct wlr_color_transform *wlr_color_transform_init_linear_to_inverse_eotf(
	enum wlr_color_transfer_function tf);

/**
 * Initialize a color transformation to apply three 1D look-up tables. dim
 * is the number of elements in each individual LUT. Returns NULL on failure.
 */
struct wlr_color_transform *wlr_color_transform_init_lut_3x1d(size_t dim,
	const uint16_t *r, const uint16_t *g, const uint16_t *b);

/**
 * Initialize a color transformation to apply a 3×3 matrix. Returns NULL on
 * failure.
 */
struct wlr_color_transform *wlr_color_transform_init_matrix(const float matrix[9]);

/**
 * Initialize a color transformation to apply a sequence of color transforms
 * one after another.
 */
struct wlr_color_transform *wlr_color_transform_init_pipeline(
	struct wlr_color_transform **transforms, size_t len);

/**
 * Increase the reference count of the color transform by 1.
 */
struct wlr_color_transform *wlr_color_transform_ref(struct wlr_color_transform *tr);

/**
 * Reduce the reference count of the color transform by 1; freeing it and
 * all associated resources when the reference count hits zero.
 */
void wlr_color_transform_unref(struct wlr_color_transform *tr);

/**
 * Evaluate a color transform for a given RGB triplet.
 */
void wlr_color_transform_eval(struct wlr_color_transform *tr,
        float out[3], const float in[3]);

/**
 * Obtain primaries values from a well-known primaries name.
 */
void wlr_color_primaries_from_named(struct wlr_color_primaries *out,
	enum wlr_color_named_primaries named);

/**
 * Compute the matrix to convert between two linear RGB color spaces
 */
void wlr_color_primaries_transform_absolute_colorimetric(
        const struct wlr_color_primaries *source,
        const struct wlr_color_primaries *destination, float matrix[9]);

#endif
