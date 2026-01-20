/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_TRANSIENT_SEAT_V1_H
#define WLR_TYPES_WLR_TRANSIENT_SEAT_V1_H

#include <wayland-server-core.h>

struct wlr_seat;

struct wlr_transient_seat_v1 {
	struct wl_resource *resource;
	struct wlr_seat *seat;

	struct {
		struct wl_listener seat_destroy;
	} WLR_PRIVATE;
};

struct wlr_transient_seat_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;

		/**
		 * Upon receiving this signal, call
		 * wlr_transient_seat_v1_ready() to pass a newly created seat
		 * to the manager, or
		 * wlr_transient_seat_v1_deny() to deny the request to create
		 * a seat.
		 */
		struct wl_signal create_seat; // struct wlr_transient_seat_v1
	} events;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_transient_seat_manager_v1 *wlr_transient_seat_manager_v1_create(
		struct wl_display *display);

/**
 * To be called when the create_seat event is received.
 *
 * This signals that the seat was successfully added and is ready.
 *
 * When the transient seat is destroyed by the client, the wlr_seat will be
 * destroyed. The wlr_seat may also be destroyed from elsewhere, in which case
 * the transient seat will become inert.
 */
void wlr_transient_seat_v1_ready(struct wlr_transient_seat_v1 *seat,
		struct wlr_seat *wlr_seat);

/**
 * To be called when the create_seat event is received.
 *
 * This signals that the compositor has denied the user's request to create a
 * transient seat.
 */
void wlr_transient_seat_v1_deny(struct wlr_transient_seat_v1 *seat);

#endif /* WLR_TYPES_WLR_TRANSIENT_SEAT_V1_H */
