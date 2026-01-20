#ifndef UTIL_MATRIX_H
#define UTIL_MATRIX_H

#include <wayland-server-protocol.h>

struct wlr_box;

/** Writes the identity matrix into mat */
void wlr_matrix_identity(float mat[static 9]);

/** mat ← a × b */
void wlr_matrix_multiply(float mat[static 9], const float a[static 9],
	const float b[static 9]);

/** Writes a 2D translation matrix to mat of magnitude (x, y) */
void wlr_matrix_translate(float mat[static 9], float x, float y);

/** Writes a 2D scale matrix to mat of magnitude (x, y) */
void wlr_matrix_scale(float mat[static 9], float x, float y);

/** Writes a transformation matrix which applies the specified
 *  wl_output_transform to mat */
void wlr_matrix_transform(float mat[static 9],
	enum wl_output_transform transform);

/** Shortcut for the various matrix operations involved in projecting the
 *  specified wlr_box onto a given orthographic projection with a given
 *  rotation. The result is written to mat, which can be applied to each
 *  coordinate of the box to get a new coordinate from [-1,1]. */
void wlr_matrix_project_box(float mat[static 9], const struct wlr_box *box,
	enum wl_output_transform transform, const float projection[static 9]);

/**
 * Writes a 2D orthographic projection matrix to mat of (width, height) with a
 * specified wl_output_transform.
 *
 * Equivalent to glOrtho(0, width, 0, height, 1, -1) with the transform applied.
 */
void matrix_projection(float mat[static 9], int width, int height,
	enum wl_output_transform transform);

/**
 * Compute the inverse of a matrix.
 *
 * The matrix needs to be inversible.
 */
void matrix_invert(float out[static 9], float m[static 9]);

#endif
