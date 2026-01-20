/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_SUBCOMPOSITOR_H
#define WLR_TYPES_WLR_SUBCOMPOSITOR_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>

/**
 * The sub-surface state describing the sub-surface's relationship with its
 * parent. Contrary to other states, this one is not applied on surface commit.
 * Instead, it's applied on parent surface commit.
 */
struct wlr_subsurface_parent_state {
	int32_t x, y;
	struct wl_list link;

	struct {
		struct wlr_surface_synced *synced;
	} WLR_PRIVATE;
};

struct wlr_subsurface {
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wlr_surface *parent;

	struct wlr_subsurface_parent_state current, pending;

	uint32_t cached_seq;
	bool has_cache;

	bool synchronized;
	bool added;

	struct {
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		struct wlr_surface_synced parent_synced;

		struct wl_listener surface_client_commit;
		struct wl_listener parent_destroy;
	} WLR_PRIVATE;
};

struct wlr_subcompositor {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

/**
 * Get a struct wlr_subsurface from a struct wlr_surface.
 *
 * Returns NULL if the surface doesn't have the subsurface role or if
 * the subsurface has been destroyed.
 */
struct wlr_subsurface *wlr_subsurface_try_from_wlr_surface(
	struct wlr_surface *surface);

struct wlr_subcompositor *wlr_subcompositor_create(struct wl_display *display);

#endif
