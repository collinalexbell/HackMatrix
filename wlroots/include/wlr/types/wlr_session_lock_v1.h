/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_SESSION_LOCK_H
#define WLR_TYPES_WLR_SESSION_LOCK_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>

struct wlr_session_lock_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal new_lock; // struct wlr_session_lock_v1
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_session_lock_v1 {
	struct wl_resource *resource;

	struct wl_list surfaces; // struct wlr_session_lock_surface_v1.link

	struct {
		struct wl_signal new_surface; // struct wlr_session_lock_surface_v1
		struct wl_signal unlock;
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		bool locked_sent;
	} WLR_PRIVATE;
};

struct wlr_session_lock_surface_v1_state {
	uint32_t width, height;
	uint32_t configure_serial;
};

struct wlr_session_lock_surface_v1_configure {
	struct wl_list link; // wlr_session_lock_surface_v1.configure_list
	uint32_t serial;

	uint32_t width, height;
};

struct wlr_session_lock_surface_v1 {
	struct wl_resource *resource;
	struct wl_list link; // wlr_session_lock_v1.surfaces

	struct wlr_output *output;
	struct wlr_surface *surface;

	bool configured;

	struct wl_list configure_list; // wlr_session_lock_surface_v1_configure.link

	struct wlr_session_lock_surface_v1_state current;
	struct wlr_session_lock_surface_v1_state pending;

	struct {
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		struct wlr_surface_synced synced;

		struct wl_listener output_destroy;
	} WLR_PRIVATE;
};

struct wlr_session_lock_manager_v1 *wlr_session_lock_manager_v1_create(
	struct wl_display *display);

void wlr_session_lock_v1_send_locked(struct wlr_session_lock_v1 *lock);
void wlr_session_lock_v1_destroy(struct wlr_session_lock_v1 *lock);

uint32_t wlr_session_lock_surface_v1_configure(
	struct wlr_session_lock_surface_v1 *lock_surface,
	uint32_t width, uint32_t height);

/**
 * Get a struct wlr_session_lock_surface_v1 from a struct wlr_surface.
 *
 * Returns NULL if the surface has a different role or if the lock surface
 * has been destroyed.
 */
struct wlr_session_lock_surface_v1 *wlr_session_lock_surface_v1_try_from_wlr_surface(
	struct wlr_surface *surface);

#endif
