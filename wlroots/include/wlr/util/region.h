/*
 * This is a stable interface of wlroots. Future changes will be limited to:
 *
 * - New functions
 * - New struct members
 * - New enum members
 *
 * Note that wlroots does not make an ABI compatibility promise - in the future,
 * the layout and size of structs used by wlroots may change, requiring code
 * depending on this header to be recompiled (but not edited).
 *
 * Breaking changes are announced in the release notes and follow a 1-year
 * deprecation schedule.
 */

#ifndef WLR_UTIL_REGION_H
#define WLR_UTIL_REGION_H

#include <stdbool.h>
#include <pixman.h>
#include <wayland-server-protocol.h>

/**
 * Scale a region by the specified factor.
 *
 * The resulting coordinates are rounded up or down so that the new region is
 * at least as big as the original one if the scale factor is greater or equal
 * to 1.
 *
 * Also see wlr_region_scale_xy().
 */
void wlr_region_scale(pixman_region32_t *dst, const pixman_region32_t *src,
	float scale);

/**
 * Scale a region by the specified factors.
 *
 * The X and Y coordinates are scaled separately by scale_x and scale_y.
 *
 * The resulting coordinates are rounded up or down so that the new region is
 * at least as big as the original one if the scale factor is greater or equal
 * to 1.
 */
void wlr_region_scale_xy(pixman_region32_t *dst, const pixman_region32_t *src,
	float scale_x, float scale_y);

/**
 * Applies a transform to a region inside a box of size `width` x `height`.
 */
void wlr_region_transform(pixman_region32_t *dst, const pixman_region32_t *src,
	enum wl_output_transform transform, int width, int height);

/**
 * Expands the region by distance on both axis. distance must be
 * a non-negative number.
 */
void wlr_region_expand(pixman_region32_t *dst, const pixman_region32_t *src,
	int distance);

/*
 * Builds the smallest possible region that contains the region rotated about
 * the point (ox, oy).
 */
void wlr_region_rotated_bounds(pixman_region32_t *dst, const pixman_region32_t *src,
	float rotation, int ox, int oy);

/**
 * Confine a point inside a region.
 *
 * x1 and y1 are the old position, x2 and y2 are the new tentative position.
 * The function returns true with confined coordinates in x2_out and y2_out if
 * the old position is within the region, or false otherwise.
 */
bool wlr_region_confine(const pixman_region32_t *region, double x1, double y1, double x2,
	double y2, double *x2_out, double *y2_out);

#endif
