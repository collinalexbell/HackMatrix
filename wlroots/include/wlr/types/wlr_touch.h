/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_TOUCH_H
#define WLR_TYPES_WLR_TOUCH_H

#include <stdint.h>
#include <wlr/types/wlr_input_device.h>
#include <wayland-server-core.h>

struct wlr_touch_impl;

struct wlr_touch {
	struct wlr_input_device base;

	const struct wlr_touch_impl *impl;

	char *output_name;
	double width_mm, height_mm;

	struct {
		struct wl_signal down; // struct wlr_touch_down_event
		struct wl_signal up; // struct wlr_touch_up_event
		struct wl_signal motion; // struct wlr_touch_motion_event
		struct wl_signal cancel; // struct wlr_touch_cancel_event
		struct wl_signal frame;
	} events;

	void *data;
};

struct wlr_touch_down_event {
	struct wlr_touch *touch;
	uint32_t time_msec;
	int32_t touch_id;
	// From 0..1
	double x, y;
};

struct wlr_touch_up_event {
	struct wlr_touch *touch;
	uint32_t time_msec;
	int32_t touch_id;
};

struct wlr_touch_motion_event {
	struct wlr_touch *touch;
	uint32_t time_msec;
	int32_t touch_id;
	// From 0..1
	double x, y;
};

struct wlr_touch_cancel_event {
	struct wlr_touch *touch;
	uint32_t time_msec;
	int32_t touch_id;
};

/**
 * Get a struct wlr_touch from a struct wlr_input_device.
 *
 * Asserts that the input device is a touch device.
 */
struct wlr_touch *wlr_touch_from_input_device(
	struct wlr_input_device *input_device);

#endif
