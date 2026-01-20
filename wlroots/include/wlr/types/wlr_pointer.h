/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_POINTER_H
#define WLR_TYPES_WLR_POINTER_H

#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wlr/types/wlr_input_device.h>

#define WLR_POINTER_BUTTONS_CAP 16

struct wlr_pointer_impl;

struct wlr_pointer {
	struct wlr_input_device base;

	const struct wlr_pointer_impl *impl;

	char *output_name;

	uint32_t buttons[WLR_POINTER_BUTTONS_CAP];
	size_t button_count;

	struct {
		struct wl_signal motion; // struct wlr_pointer_motion_event
		struct wl_signal motion_absolute; // struct wlr_pointer_motion_absolute_event
		struct wl_signal button; // struct wlr_pointer_button_event
		struct wl_signal axis; // struct wlr_pointer_axis_event
		struct wl_signal frame;

		struct wl_signal swipe_begin; // struct wlr_pointer_swipe_begin_event
		struct wl_signal swipe_update; // struct wlr_pointer_swipe_update_event
		struct wl_signal swipe_end; // struct wlr_pointer_swipe_end_event

		struct wl_signal pinch_begin; // struct wlr_pointer_pinch_begin_event
		struct wl_signal pinch_update; // struct wlr_pointer_pinch_update_event
		struct wl_signal pinch_end; // struct wlr_pointer_pinch_end_event

		struct wl_signal hold_begin; // struct wlr_pointer_hold_begin_event
		struct wl_signal hold_end; // struct wlr_pointer_hold_end_event
	} events;

	void *data;
};

struct wlr_pointer_motion_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	double delta_x, delta_y;
	double unaccel_dx, unaccel_dy;
};

struct wlr_pointer_motion_absolute_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	// From 0..1
	double x, y;
};

struct wlr_pointer_button_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	uint32_t button;
	enum wl_pointer_button_state state;
};

#define WLR_POINTER_AXIS_DISCRETE_STEP 120

struct wlr_pointer_axis_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	enum wl_pointer_axis_source source;
	enum wl_pointer_axis orientation;
	enum wl_pointer_axis_relative_direction relative_direction;
	double delta;
	int32_t delta_discrete;
};

struct wlr_pointer_swipe_begin_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	uint32_t fingers;
};

struct wlr_pointer_swipe_update_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	uint32_t fingers;
	// Relative coordinates of the logical center of the gesture
	// compared to the previous event.
	double dx, dy;
};

struct wlr_pointer_swipe_end_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	bool cancelled;
};

struct wlr_pointer_pinch_begin_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	uint32_t fingers;
};

struct wlr_pointer_pinch_update_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	uint32_t fingers;
	// Relative coordinates of the logical center of the gesture
	// compared to the previous event.
	double dx, dy;
	// Absolute scale compared to the begin event
	double scale;
	// Relative angle in degrees clockwise compared to the previous event.
	double rotation;
};

struct wlr_pointer_pinch_end_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	bool cancelled;
};

struct wlr_pointer_hold_begin_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	uint32_t fingers;
};

struct wlr_pointer_hold_end_event {
	struct wlr_pointer *pointer;
	uint32_t time_msec;
	bool cancelled;
};

/**
 * Get a struct wlr_pointer from a struct wlr_input_device.
 *
 * Asserts that the input device is a pointer.
 */
struct wlr_pointer *wlr_pointer_from_input_device(
	struct wlr_input_device *input_device);

#endif
