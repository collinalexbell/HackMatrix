#ifndef UTIL_RECT_UNION_H
#define UTIL_RECT_UNION_H

#include <stdlib.h>
#include <stdbool.h>
#include <pixman.h>
#include <wayland-util.h>

/**
 * `struct rect_union` is a data structure to efficiently accumulate a number
 * of rectangles and then, when needed, compute a disjoint cover of their union.
 * (That is: produce a list of disjoint rectangles which covers every point
 * that was contained in one of the added rectangles.)
 *
 * Add rectangles to the union with `rect_union_add()`; to compute the disjoint
 * union, run `rect_union_evaluate()`, which will place the result in `.region`.
 * If there were any allocation failures, `.region` will instead contain the
 * bounding box for the entire list of rectangles.
 *
 * Example usage:
 *
 *     struct rect_union r;
 *     rect_union_init(&r);
 *     for (j in ...) {
 *         rect_union_add(&r, box[j]);
 *     }
 *     const pixman_region32_t *reg = rect_union_evaluate(&r);
 *     int nboxes;
 *     pixman_box32_t *boxes = pixman_region32_rectangles(reg, nboxes);
 *     for (int i = 0; i < nboxes; i++) {
 *         do_stuff(boxes[i]);
 *     }
 *     rect_union_destroy(&r);
 *
 */
struct rect_union {
	pixman_box32_t bounding_box; // Always up-to-date bounding box
	pixman_region32_t region; // Updated only on _evaluate()

	struct wl_array unsorted; // pixman_box32_t
	bool alloc_failure; // If this is true, fall back to computing a bounding box
};

/**
 * Initialize *r, disregarding any previous contents.
 */
void rect_union_init(struct rect_union *r);

/**
 * Free heap data associated with *r; should only be called after rect_union_init.
 * Leaves *r in an invalid state.
 */
void rect_union_finish(struct rect_union *r);

/**
 * Add a rectangle to the union. If `box` is empty or invalid (x2 > x1 || y2 > y1),
 * do nothing.
 *
 * Amortized time: O(1)
 */
void rect_union_add(struct rect_union *r, pixman_box32_t box);

/**
 * Compute an exact cover of the rectangles added so far, and return
 * a pointer to a pixman_region32_t giving that cover. The pointer will
 * remain valid until the next time *r is modified. If there was an allocation
 * failure, this function may return a single-rectangle bounding box instead.
 *
 * This may be called multiple times and interleaved with rect_union_add().
 *
 * Worst case time: O(t^2), where t is the number of rectangles in the list.
 * Best case time: O(t), if rectangles are disjoint and have y-x band structure
 */
const pixman_region32_t *rect_union_evaluate(struct rect_union *r);

#endif
