
#include <libinput.h>
#include <stdlib.h>
#include <wlr/backend/session.h>
#include <wlr/interfaces/wlr_keyboard.h>
#include <wlr/interfaces/wlr_pointer.h>
#include <wlr/interfaces/wlr_touch.h>
#include <wlr/interfaces/wlr_tablet_tool.h>
#include <wlr/interfaces/wlr_tablet_pad.h>
#include <wlr/interfaces/wlr_switch.h>
#include <wlr/util/log.h>
#include "backend/libinput.h"

void destroy_libinput_input_device(struct wlr_libinput_input_device *dev) {
	if (dev->keyboard.impl) {
		wlr_keyboard_finish(&dev->keyboard);
	}
	if (dev->pointer.impl) {
		wlr_pointer_finish(&dev->pointer);
	}
	if (dev->switch_device.impl) {
		wlr_switch_finish(&dev->switch_device);
	}
	if (dev->touch.impl) {
		wlr_touch_finish(&dev->touch);
	}
	if (dev->tablet.impl) {
		finish_device_tablet(dev);
	}
	if (dev->tablet_pad.impl) {
		finish_device_tablet_pad(dev);
	}

	libinput_device_unref(dev->handle);
	wl_list_remove(&dev->link);
	free(dev);
}

bool wlr_input_device_is_libinput(struct wlr_input_device *wlr_dev) {
	switch (wlr_dev->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
		return wlr_keyboard_from_input_device(wlr_dev)->impl ==
			&libinput_keyboard_impl;
	case WLR_INPUT_DEVICE_POINTER:
		return wlr_pointer_from_input_device(wlr_dev)->impl ==
			&libinput_pointer_impl;
	case WLR_INPUT_DEVICE_TOUCH:
		return wlr_touch_from_input_device(wlr_dev)->impl ==
			&libinput_touch_impl;
	case WLR_INPUT_DEVICE_TABLET:
		return wlr_tablet_from_input_device(wlr_dev)->impl ==
			&libinput_tablet_impl;
	case WLR_INPUT_DEVICE_TABLET_PAD:
		return wlr_tablet_pad_from_input_device(wlr_dev)->impl ==
			&libinput_tablet_pad_impl;
	case WLR_INPUT_DEVICE_SWITCH:
		return wlr_switch_from_input_device(wlr_dev)->impl ==
			&libinput_switch_impl;
	default:
		return false;
	}
}

static void handle_device_added(struct wlr_libinput_backend *backend,
		struct libinput_device *libinput_dev) {
	int vendor = libinput_device_get_id_vendor(libinput_dev);
	int product = libinput_device_get_id_product(libinput_dev);
	const char *name = libinput_device_get_name(libinput_dev);
	wlr_log(WLR_DEBUG, "Adding %s [%d:%d]", name, vendor, product);

	struct wlr_libinput_input_device *dev = calloc(1, sizeof(*dev));
	if (dev == NULL) {
		wlr_log_errno(WLR_ERROR, "failed to allocate wlr_libinput_input_device");
		return;
	}

	dev->handle = libinput_dev;
	libinput_device_ref(libinput_dev);
	libinput_device_set_user_data(libinput_dev, dev);

	wl_list_insert(&backend->devices, &dev->link);

	if (libinput_device_has_capability(
			libinput_dev, LIBINPUT_DEVICE_CAP_KEYBOARD)) {
		init_device_keyboard(dev);

		wl_signal_emit_mutable(&backend->backend.events.new_input,
			&dev->keyboard.base);
	}

	if (libinput_device_has_capability(
			libinput_dev, LIBINPUT_DEVICE_CAP_POINTER)) {
		init_device_pointer(dev);

		wl_signal_emit_mutable(&backend->backend.events.new_input,
			&dev->pointer.base);
	}

	if (libinput_device_has_capability(
			libinput_dev, LIBINPUT_DEVICE_CAP_SWITCH)) {
		init_device_switch(dev);
		wl_signal_emit_mutable(&backend->backend.events.new_input,
			&dev->switch_device.base);
	}

	if (libinput_device_has_capability(
			libinput_dev, LIBINPUT_DEVICE_CAP_TOUCH)) {
		init_device_touch(dev);
		wl_signal_emit_mutable(&backend->backend.events.new_input,
			&dev->touch.base);
	}

	if (libinput_device_has_capability(libinput_dev,
			LIBINPUT_DEVICE_CAP_TABLET_TOOL)) {
		init_device_tablet(dev);
		wl_signal_emit_mutable(&backend->backend.events.new_input,
			&dev->tablet.base);
	}

	if (libinput_device_has_capability(
			libinput_dev, LIBINPUT_DEVICE_CAP_TABLET_PAD)) {
		init_device_tablet_pad(dev);
		wl_signal_emit_mutable(&backend->backend.events.new_input,
			&dev->tablet_pad.base);
	}
}

static void handle_device_removed(struct wlr_libinput_backend *backend,
		struct wlr_libinput_input_device *dev) {
	int vendor = libinput_device_get_id_vendor(dev->handle);
	int product = libinput_device_get_id_product(dev->handle);
	const char *name = libinput_device_get_name(dev->handle);
	wlr_log(WLR_DEBUG, "Removing %s [%d:%d]", name, vendor, product);

	destroy_libinput_input_device(dev);
}

void handle_libinput_event(struct wlr_libinput_backend *backend,
		struct libinput_event *event) {
	struct libinput_device *libinput_dev = libinput_event_get_device(event);
	struct wlr_libinput_input_device *dev =
		libinput_device_get_user_data(libinput_dev);
	enum libinput_event_type event_type = libinput_event_get_type(event);

	if (dev == NULL && event_type != LIBINPUT_EVENT_DEVICE_ADDED) {
		wlr_log(WLR_ERROR, "libinput_device has no wlr_libinput_input_device");
		return;
	}

	switch (event_type) {
	case LIBINPUT_EVENT_DEVICE_ADDED:
		handle_device_added(backend, libinput_dev);
		break;
	case LIBINPUT_EVENT_DEVICE_REMOVED:
		handle_device_removed(backend, dev);
		break;
	case LIBINPUT_EVENT_KEYBOARD_KEY:
		handle_keyboard_key(event, &dev->keyboard);
		break;
	case LIBINPUT_EVENT_POINTER_MOTION:
		handle_pointer_motion(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE:
		handle_pointer_motion_abs(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_POINTER_BUTTON:
		handle_pointer_button(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_POINTER_AXIS:
		/* This event must be ignored in favour of the SCROLL_* events */
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
		handle_pointer_axis_value120(event, &dev->pointer,
			WL_POINTER_AXIS_SOURCE_WHEEL);
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
		handle_pointer_axis_value120(event, &dev->pointer,
			WL_POINTER_AXIS_SOURCE_FINGER);
		break;
	case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
		handle_pointer_axis_value120(event, &dev->pointer,
			WL_POINTER_AXIS_SOURCE_CONTINUOUS);
		break;
	case LIBINPUT_EVENT_TOUCH_DOWN:
		handle_touch_down(event, &dev->touch);
		break;
	case LIBINPUT_EVENT_TOUCH_UP:
		handle_touch_up(event, &dev->touch);
		break;
	case LIBINPUT_EVENT_TOUCH_MOTION:
		handle_touch_motion(event, &dev->touch);
		break;
	case LIBINPUT_EVENT_TOUCH_CANCEL:
		handle_touch_cancel(event, &dev->touch);
		break;
	case LIBINPUT_EVENT_TOUCH_FRAME:
		handle_touch_frame(event, &dev->touch);
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_AXIS:
		handle_tablet_tool_axis(event, &dev->tablet);
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_PROXIMITY:
		handle_tablet_tool_proximity(event, &dev->tablet);
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_TIP:
		handle_tablet_tool_tip(event, &dev->tablet);
		break;
	case LIBINPUT_EVENT_TABLET_TOOL_BUTTON:
		handle_tablet_tool_button(event, &dev->tablet);
		break;
	case LIBINPUT_EVENT_TABLET_PAD_BUTTON:
		handle_tablet_pad_button(event, &dev->tablet_pad);
		break;
	case LIBINPUT_EVENT_TABLET_PAD_RING:
		handle_tablet_pad_ring(event, &dev->tablet_pad);
		break;
	case LIBINPUT_EVENT_TABLET_PAD_STRIP:
		handle_tablet_pad_strip(event, &dev->tablet_pad);
		break;
	case LIBINPUT_EVENT_SWITCH_TOGGLE:
		handle_switch_toggle(event, &dev->switch_device);
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_BEGIN:
		handle_pointer_swipe_begin(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE:
		handle_pointer_swipe_update(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_GESTURE_SWIPE_END:
		handle_pointer_swipe_end(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_BEGIN:
		handle_pointer_pinch_begin(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_UPDATE:
		handle_pointer_pinch_update(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_GESTURE_PINCH_END:
		handle_pointer_pinch_end(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_GESTURE_HOLD_BEGIN:
		handle_pointer_hold_begin(event, &dev->pointer);
		break;
	case LIBINPUT_EVENT_GESTURE_HOLD_END:
		handle_pointer_hold_end(event, &dev->pointer);
		break;
	default:
		wlr_log(WLR_DEBUG, "Unknown libinput event %d", event_type);
		break;
	}
}
