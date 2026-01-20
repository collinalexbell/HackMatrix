#ifndef WLR_BACKEND_SESSION_H
#define WLR_BACKEND_SESSION_H

#include <stdbool.h>
#include <sys/types.h>
#include <wayland-server-core.h>

struct libseat;

/**
 * An opened physical device.
 */
struct wlr_device {
	int fd;
	int device_id;
	dev_t dev;
	struct wl_list link; // wlr_session.devices

	struct {
		struct wl_signal change; // struct wlr_device_change_event
		struct wl_signal remove;
	} events;
};

/**
 * A session manages access to physical devices (such as GPUs and input
 * devices).
 *
 * A session is only required when running on bare metal (e.g. with the KMS or
 * libinput backends).
 *
 * The session listens for device hotplug events, and relays that information
 * via the add_drm_card event and the change/remove events on struct wlr_device.
 * The session provides functions to gain access to physical device (which is a
 * privileged operation), see wlr_session_open_file(). The session also keeps
 * track of the virtual terminal state (allowing users to switch between
 * compositors or TTYs), see wlr_session_change_vt() and the active event.
 */
struct wlr_session {
	/*
	 * Signal for when the session becomes active/inactive.
	 * It's called when we swap virtual terminal.
	 */
	bool active;

	char seat[256];

	struct udev *udev;
	struct udev_monitor *mon;
	struct wl_event_source *udev_event;

	struct libseat *seat_handle;
	struct wl_event_source *libseat_event;

	struct wl_list devices; // wlr_device.link

	struct wl_event_loop *event_loop;

	struct {
		struct wl_signal active;
		struct wl_signal add_drm_card; // struct wlr_session_add_event
		struct wl_signal destroy;
	} events;

	struct {
		struct wl_listener event_loop_destroy;
	} WLR_PRIVATE;
};

struct wlr_session_add_event {
	const char *path;
};

enum wlr_device_change_type {
	WLR_DEVICE_HOTPLUG = 1,
	WLR_DEVICE_LEASE,
};

struct wlr_device_hotplug_event {
	uint32_t connector_id;
	uint32_t prop_id;
};

struct wlr_device_change_event {
	enum wlr_device_change_type type;
	union {
		struct wlr_device_hotplug_event hotplug;
	};
};

/*
 * Opens a session, taking control of the current virtual terminal.
 * This should not be called if another program is already in control
 * of the terminal (Xorg, another Wayland compositor, etc.).
 *
 * Returns NULL on error.
 */
struct wlr_session *wlr_session_create(struct wl_event_loop *loop);

/*
 * Closes a previously opened session and restores the virtual terminal.
 * You should call wlr_session_close_file() on each files you opened
 * with wlr_session_open_file() before you call this.
 */
void wlr_session_destroy(struct wlr_session *session);

/*
 * Opens the file at path.
 *
 * This can only be used to open DRM or evdev (input) devices. Files opened via
 * this function must be closed by calling wlr_session_close_file().
 *
 * When the session becomes inactive:
 *
 * - DRM files lose their DRM master status
 * - evdev files become invalid and should be closed
 */
struct wlr_device *wlr_session_open_file(struct wlr_session *session,
	const char *path);

/*
 * Closes a file previously opened with wlr_session_open_file().
 */
void wlr_session_close_file(struct wlr_session *session,
	struct wlr_device *device);

/*
 * Changes the virtual terminal.
 */
bool wlr_session_change_vt(struct wlr_session *session, unsigned vt);

/**
 * Enumerate and open KMS devices.
 *
 * ret is filled with up to ret_len devices. The number of devices ret has been
 * filled with is returned on success. If more devices than ret_len are probed,
 * the extraneous ones are ignored. If there is no KMS device, the function
 * will block until such device is detected up to a timeout. The first device
 * returned is the default device (marked as "boot_vga" by the kernel).
 *
 * On error, or if no device was found, -1 is returned.
 */
ssize_t wlr_session_find_gpus(struct wlr_session *session,
	size_t ret_len, struct wlr_device **ret);

#endif
