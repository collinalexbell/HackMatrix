#include <limits.h>
#include "util/rect_union.h"

static void box_union(pixman_box32_t *dst, pixman_box32_t box) {
	dst->x1 = dst->x1 < box.x1 ? dst->x1 : box.x1;
	dst->y1 = dst->y1 < box.y1 ? dst->y1 : box.y1;
	dst->x2 = dst->x2 > box.x2 ? dst->x2 : box.x2;
	dst->y2 = dst->y2 > box.y2 ? dst->y2 : box.y2;
}

static bool box_empty_or_invalid(pixman_box32_t box) {
	return box.x1 >= box.x2 || box.y1 >= box.y2;
}

void rect_union_init(struct rect_union *ru) {
	*ru = (struct rect_union) {
		.alloc_failure = false,
		.bounding_box = (pixman_box32_t) {
			.x1 = INT_MAX,
			.x2 = INT_MIN,
			.y1 = INT_MAX,
			.y2 = INT_MIN,
		}
	};
	pixman_region32_init(&ru->region);
	wl_array_init(&ru->unsorted);
};

void rect_union_finish(struct rect_union *ru) {
	pixman_region32_fini(&ru->region);
	wl_array_release(&ru->unsorted);
}

static void handle_alloc_failure(struct rect_union *ru) {
	ru->alloc_failure = true;
	wl_array_release(&ru->unsorted);
	wl_array_init(&ru->unsorted);
}

void rect_union_add(struct rect_union *ru, pixman_box32_t box) {
	if (box_empty_or_invalid(box)) {
		return;
	}

	box_union(&ru->bounding_box, box);

	if (!ru->alloc_failure) {
		pixman_box32_t *entry = wl_array_add(&ru->unsorted, sizeof(*entry));
		if (entry) {
			*entry = box;
		} else {
			handle_alloc_failure(ru);
		}
	}
}

const pixman_region32_t *rect_union_evaluate(struct rect_union *ru) {
	if (ru->alloc_failure) {
		goto bounding_box;
	}

	int nrects = (int)(ru->unsorted.size / sizeof(pixman_box32_t));
	pixman_region32_t reg;
	bool ok = pixman_region32_init_rects(&reg, ru->unsorted.data, nrects);
	if (!ok) {
		handle_alloc_failure(ru);
		goto bounding_box;
	}
	ok = pixman_region32_union(&reg, &reg, &ru->region);
	if (!ok) {
		pixman_region32_fini(&reg);
		handle_alloc_failure(ru);
		goto bounding_box;
	}
	pixman_region32_fini(&ru->region);
	// pixman_region32_t is safe to move
	ru->region = reg;
	wl_array_release(&ru->unsorted);
	wl_array_init(&ru->unsorted);

	return &ru->region;
bounding_box:
	pixman_region32_fini(&ru->region);
	if (box_empty_or_invalid(ru->bounding_box)) {
		pixman_region32_init(&ru->region);
	} else {
		pixman_region32_init_with_extents(&ru->region, &ru->bounding_box);
	}
	return &ru->region;
}

