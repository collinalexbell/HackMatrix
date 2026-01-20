/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_IDLE_NOTIFY_H
#define WLR_TYPES_WLR_IDLE_NOTIFY_H

#include <wayland-server-core.h>

struct wlr_seat;

/**
 * An idle notifier, implementing the ext-idle-notify-v1 protocol.
 */
struct wlr_idle_notifier_v1 {
	struct wl_global *global;

	struct {
		bool inhibited;
		struct wl_list notifications; // wlr_idle_notification_v1.link

		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};


/**
 * Create the ext_idle_notifier_v1 global.
 */
struct wlr_idle_notifier_v1 *wlr_idle_notifier_v1_create(struct wl_display *display);

/**
 * Inhibit idle.
 *
 * Compositors should call this function when the idle state is disabled, e.g.
 * because a visible client is using the idle-inhibit protocol.
 */
void wlr_idle_notifier_v1_set_inhibited(struct wlr_idle_notifier_v1 *notifier,
	bool inhibited);

/**
 * Notify for user activity on a seat.
 *
 * Compositors should call this function whenever an input event is triggered
 * on a seat.
 */
void wlr_idle_notifier_v1_notify_activity(struct wlr_idle_notifier_v1 *notifier,
	struct wlr_seat *seat);

#endif
