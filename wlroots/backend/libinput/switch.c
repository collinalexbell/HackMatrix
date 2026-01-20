#include <assert.h>
#include <libinput.h>
#include <wlr/interfaces/wlr_switch.h>
#include "backend/libinput.h"

const struct wlr_switch_impl libinput_switch_impl = {
	.name = "libinput-switch",
};

void init_device_switch(struct wlr_libinput_input_device *dev) {
	const char *name = get_libinput_device_name(dev->handle);
	struct wlr_switch *wlr_switch = &dev->switch_device;
	wlr_switch_init(wlr_switch, &libinput_switch_impl, name);
}

struct wlr_libinput_input_device *device_from_switch(
		struct wlr_switch *wlr_switch) {
	assert(wlr_switch->impl == &libinput_switch_impl);

	struct wlr_libinput_input_device *dev =
		wl_container_of(wlr_switch, dev, switch_device);
	return dev;
}

void handle_switch_toggle(struct libinput_event *event,
		struct wlr_switch *wlr_switch) {
	struct libinput_event_switch *sevent =
		libinput_event_get_switch_event	(event);
	struct wlr_switch_toggle_event wlr_event = {
		.time_msec = usec_to_msec(libinput_event_switch_get_time_usec(sevent)),
	};
	switch (libinput_event_switch_get_switch(sevent)) {
	case LIBINPUT_SWITCH_LID:
		wlr_event.switch_type = WLR_SWITCH_TYPE_LID;
		break;
	case LIBINPUT_SWITCH_TABLET_MODE:
		wlr_event.switch_type = WLR_SWITCH_TYPE_TABLET_MODE;
		break;
	}
	switch (libinput_event_switch_get_switch_state(sevent)) {
	case LIBINPUT_SWITCH_STATE_OFF:
		wlr_event.switch_state = WLR_SWITCH_STATE_OFF;
		break;
	case LIBINPUT_SWITCH_STATE_ON:
		wlr_event.switch_state = WLR_SWITCH_STATE_ON;
		break;
	}
	wl_signal_emit_mutable(&wlr_switch->events.toggle, &wlr_event);
}
