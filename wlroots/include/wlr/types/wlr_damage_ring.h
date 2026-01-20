/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_DAMAGE_RING_H
#define WLR_TYPES_WLR_DAMAGE_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pixman.h>
#include <wayland-server-core.h>

struct wlr_box;

struct wlr_damage_ring_buffer {
	struct wlr_buffer *buffer;
	pixman_region32_t damage;

	struct wlr_damage_ring *ring;
	struct wl_list link; // wlr_damage_ring.buffers

	struct {
		struct wl_listener destroy;
	} WLR_PRIVATE;
};

struct wlr_damage_ring {
	// Difference between the current buffer and the previous one
	pixman_region32_t current;

	struct {
		struct wl_list buffers; // wlr_damage_ring_buffer.link
	} WLR_PRIVATE;
};

void wlr_damage_ring_init(struct wlr_damage_ring *ring);

void wlr_damage_ring_finish(struct wlr_damage_ring *ring);

/**
 * Add a region to the current damage. The region must be in the buffer-local
 * coordinate space.
 */
void wlr_damage_ring_add(struct wlr_damage_ring *ring,
	const pixman_region32_t *damage);

/**
 * Add a box to the current damage. The box must be in the buffer-local
 * coordinate space.
 */
void wlr_damage_ring_add_box(struct wlr_damage_ring *ring,
	const struct wlr_box *box);

/**
 * Damage the ring fully.
 */
void wlr_damage_ring_add_whole(struct wlr_damage_ring *ring);

/**
 * Get accumulated buffer damage and rotate the damage ring.
 *
 * The accumulated buffer damage is the difference between the to-be-painted
 * buffer and the passed-in buffer. In other words, this is the region that
 * needs to be redrawn.
 *
 * Users should damage the ring if an error occurs while rendering or
 * submitting the new buffer to the backend.
 *
 * The returned damage will be in the buffer-local coordinate space.
 */
void wlr_damage_ring_rotate_buffer(struct wlr_damage_ring *ring,
	struct wlr_buffer *buffer, pixman_region32_t *damage);

#endif
