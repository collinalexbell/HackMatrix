#include <assert.h>
#include <libinput.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include "backend/libinput.h"

struct wlr_libinput_input_device *device_from_keyboard(
		struct wlr_keyboard *kb) {
	assert(kb->impl == &libinput_keyboard_impl);

	struct wlr_libinput_input_device *dev = wl_container_of(kb, dev, keyboard);
	return dev;
}

static void keyboard_set_leds(struct wlr_keyboard *wlr_kb, uint32_t leds) {
	struct wlr_libinput_input_device *dev = device_from_keyboard(wlr_kb);
	libinput_device_led_update(dev->handle, leds);
}

const struct wlr_keyboard_impl libinput_keyboard_impl = {
	.name = "libinput-keyboard",
	.led_update = keyboard_set_leds
};

void init_device_keyboard(struct wlr_libinput_input_device *dev) {
	const char *name = get_libinput_device_name(dev->handle);
	struct wlr_keyboard *wlr_kb = &dev->keyboard;
	wlr_keyboard_init(wlr_kb, &libinput_keyboard_impl, name);

	libinput_device_led_update(dev->handle, 0);
}

void handle_keyboard_key(struct libinput_event *event,
		struct wlr_keyboard *kb) {
	struct libinput_event_keyboard *kbevent =
		libinput_event_get_keyboard_event(event);
	struct wlr_keyboard_key_event wlr_event = {
		.time_msec = usec_to_msec(libinput_event_keyboard_get_time_usec(kbevent)),
		.keycode = libinput_event_keyboard_get_key(kbevent),
		.update_state = true,
	};
	switch (libinput_event_keyboard_get_key_state(kbevent)) {
	case LIBINPUT_KEY_STATE_RELEASED:
		wlr_event.state = WL_KEYBOARD_KEY_STATE_RELEASED;
		break;
	case LIBINPUT_KEY_STATE_PRESSED:
		wlr_event.state = WL_KEYBOARD_KEY_STATE_PRESSED;
		break;
	}
	wlr_keyboard_notify_key(kb, &wlr_event);
}
