/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_CURSOR_SHAPE_V1_H
#define WLR_TYPES_WLR_CURSOR_SHAPE_V1_H

#include <wayland-server-core.h>
#include <wayland-protocols/cursor-shape-v1-enum.h>

/**
 * Manager for the cursor-shape-v1 protocol.
 *
 * Compositors should listen to the request_set_shape event and handle it in
 * the same way as wlr_seat.events.request_set_cursor.
 */
struct wlr_cursor_shape_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal request_set_shape; // struct wlr_cursor_shape_manager_v1_request_set_shape_event
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

enum wlr_cursor_shape_manager_v1_device_type {
	WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_POINTER,
	WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_TABLET_TOOL,
};

struct wlr_cursor_shape_manager_v1_request_set_shape_event {
	struct wlr_seat_client *seat_client;
	enum wlr_cursor_shape_manager_v1_device_type device_type;
	// NULL if device_type is not TABLET_TOOL
	struct wlr_tablet_v2_tablet_tool *tablet_tool;
	uint32_t serial;
	enum wp_cursor_shape_device_v1_shape shape;
};

struct wlr_cursor_shape_manager_v1 *wlr_cursor_shape_manager_v1_create(
	struct wl_display *display, uint32_t version);

/**
 * Get the name of a cursor shape.
 *
 * The name can be used to load a cursor from an XCursor theme.
 */
const char *wlr_cursor_shape_v1_name(enum wp_cursor_shape_device_v1_shape shape);

#endif
