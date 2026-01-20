/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_INPUT_DEVICE_H
#define WLR_TYPES_WLR_INPUT_DEVICE_H

#include <wayland-server-core.h>

enum wlr_button_state {
	WLR_BUTTON_RELEASED,
	WLR_BUTTON_PRESSED,
};

/**
 * Type of an input device.
 */
enum wlr_input_device_type {
	WLR_INPUT_DEVICE_KEYBOARD, // struct wlr_keyboard
	WLR_INPUT_DEVICE_POINTER, // struct wlr_pointer
	WLR_INPUT_DEVICE_TOUCH, // struct wlr_touch
	WLR_INPUT_DEVICE_TABLET, // struct wlr_tablet
	WLR_INPUT_DEVICE_TABLET_PAD, // struct wlr_tablet_pad
	WLR_INPUT_DEVICE_SWITCH, // struct wlr_switch
};

/**
 * An input device.
 *
 * Depending on its type, the input device can be converted to a more specific
 * type. See the various wlr_*_from_input_device() functions.
 *
 * Input devices are typically advertised by the new_input event in
 * struct wlr_backend.
 */
struct wlr_input_device {
	enum wlr_input_device_type type;
	char *name; // may be NULL

	struct {
		struct wl_signal destroy;
	} events;

	void *data;
};

#endif
