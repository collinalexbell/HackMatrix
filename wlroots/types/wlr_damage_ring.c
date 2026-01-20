#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <pixman.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/util/box.h>

#define WLR_DAMAGE_RING_MAX_RECTS 20

void wlr_damage_ring_init(struct wlr_damage_ring *ring) {
	*ring = (struct wlr_damage_ring){ 0 };
	pixman_region32_init(&ring->current);
	wl_list_init(&ring->buffers);
}

static void buffer_destroy(struct wlr_damage_ring_buffer *entry) {
	wl_list_remove(&entry->destroy.link);
	wl_list_remove(&entry->link);
	pixman_region32_fini(&entry->damage);
	free(entry);
}

void wlr_damage_ring_finish(struct wlr_damage_ring *ring) {
	pixman_region32_fini(&ring->current);
	struct wlr_damage_ring_buffer *entry, *tmp_entry;
	wl_list_for_each_safe(entry, tmp_entry, &ring->buffers, link) {
		buffer_destroy(entry);
	}
}

void wlr_damage_ring_add(struct wlr_damage_ring *ring,
		const pixman_region32_t *damage) {
	pixman_region32_union(&ring->current, &ring->current, damage);
}

void wlr_damage_ring_add_box(struct wlr_damage_ring *ring,
		const struct wlr_box *box) {
	pixman_region32_union_rect(&ring->current,
		&ring->current, box->x, box->y,
		box->width, box->height);
}

void wlr_damage_ring_add_whole(struct wlr_damage_ring *ring) {
	int width = 0;
	int height = 0;

	struct wlr_damage_ring_buffer *entry;
	wl_list_for_each(entry, &ring->buffers, link) {
		width = width < entry->buffer->width ? entry->buffer->width : width;
		height = height < entry->buffer->height ? entry->buffer->height : height;
	}

	pixman_region32_union_rect(&ring->current,
		&ring->current, 0, 0, width, height);
}

static void entry_squash_damage(struct wlr_damage_ring_buffer *entry) {
	pixman_region32_t *prev;
	if (entry->link.prev == &entry->ring->buffers) {
		// this entry is the first in the list
		prev = &entry->ring->current;
	} else {
		struct wlr_damage_ring_buffer *last =
			wl_container_of(entry->link.prev, last, link);
		prev = &last->damage;
	}

	pixman_region32_union(prev, prev, &entry->damage);
}

static void buffer_handle_destroy(struct wl_listener *listener, void *data) {
	struct wlr_damage_ring_buffer *entry = wl_container_of(listener, entry, destroy);
	entry_squash_damage(entry);
	buffer_destroy(entry);
}

void wlr_damage_ring_rotate_buffer(struct wlr_damage_ring *ring,
		struct wlr_buffer *buffer, pixman_region32_t *damage) {
	pixman_region32_copy(damage, &ring->current);

	struct wlr_damage_ring_buffer *entry;
	wl_list_for_each(entry, &ring->buffers, link) {
		if (entry->buffer != buffer) {
			pixman_region32_union(damage, damage, &entry->damage);
			continue;
		}

		pixman_region32_intersect_rect(damage, damage, 0, 0, buffer->width, buffer->height);

		// Check the number of rectangles
		int n_rects = pixman_region32_n_rects(damage);
		if (n_rects > WLR_DAMAGE_RING_MAX_RECTS) {
			pixman_box32_t *extents = pixman_region32_extents(damage);
			pixman_region32_union_rect(damage, damage,
				extents->x1, extents->y1,
				extents->x2 - extents->x1,
				extents->y2 - extents->y1);
		}

		// rotate
		entry_squash_damage(entry);
		pixman_region32_copy(&entry->damage, &ring->current);
		pixman_region32_clear(&ring->current);

		wl_list_remove(&entry->link);
		wl_list_insert(&ring->buffers, &entry->link);
		return;
	}

	pixman_region32_clear(damage);
	pixman_region32_union_rect(damage, damage,
		0, 0, buffer->width, buffer->height);

	entry = calloc(1, sizeof(*entry));
	if (!entry) {
		return;
	}

	pixman_region32_init(&entry->damage);
	pixman_region32_copy(&entry->damage, &ring->current);
	pixman_region32_clear(&ring->current);

	wl_list_insert(&ring->buffers, &entry->link);
	entry->buffer = buffer;
	entry->ring = ring;

	entry->destroy.notify = buffer_handle_destroy;
	wl_signal_add(&buffer->events.destroy, &entry->destroy);
}
