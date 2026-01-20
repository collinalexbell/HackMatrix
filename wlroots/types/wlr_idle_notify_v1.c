#include <assert.h>
#include <stdlib.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_seat.h>
#include "ext-idle-notify-v1-protocol.h"

#define IDLE_NOTIFIER_VERSION 2

struct wlr_idle_notification_v1 {
	struct wl_resource *resource;
	struct wl_list link; // wlr_idle_notifier_v1.notifications
	struct wlr_idle_notifier_v1 *notifier;
	struct wlr_seat *seat;

	uint32_t timeout_ms;
	struct wl_event_source *timer;

	bool idle;
	bool obey_inhibitors;

	struct wl_listener seat_destroy;
};

static void resource_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct ext_idle_notifier_v1_interface notifier_impl;

static const struct ext_idle_notification_v1_interface notification_impl = {
	.destroy = resource_handle_destroy,
};

// Returns NULL if the resource is inert
static struct wlr_idle_notification_v1 *notification_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_idle_notification_v1_interface, &notification_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_idle_notifier_v1 *notifier_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_idle_notifier_v1_interface, &notifier_impl));
	return wl_resource_get_user_data(resource);
}

static void notification_set_idle(struct wlr_idle_notification_v1 *notification,
		bool idle) {
	if (notification->idle == idle) {
		return;
	}

	if (idle) {
		ext_idle_notification_v1_send_idled(notification->resource);
	} else {
		ext_idle_notification_v1_send_resumed(notification->resource);
	}

	notification->idle = idle;
}

static int notification_handle_timer(void *data) {
	struct wlr_idle_notification_v1 *notification = data;
	notification_set_idle(notification, true);
	return 0;
}

static void notification_destroy(struct wlr_idle_notification_v1 *notification) {
	if (notification == NULL) {
		return;
	}
	wl_list_remove(&notification->link);
	wl_list_remove(&notification->seat_destroy.link);
	if (notification->timer != NULL) {
		wl_event_source_remove(notification->timer);
	}
	wl_resource_set_user_data(notification->resource, NULL); // make inert
	free(notification);
}

static void notification_reset_timer(struct wlr_idle_notification_v1 *notification) {
	if (notification->notifier->inhibited && notification->obey_inhibitors) {
		notification_set_idle(notification, false);
		if (notification->timer != NULL) {
			wl_event_source_timer_update(notification->timer, 0);
		}
		return;
	}

	if (notification->timer != NULL) {
		wl_event_source_timer_update(notification->timer,
			notification->timeout_ms);
	} else {
		notification_set_idle(notification, true);
	}
}

static void notification_handle_activity(struct wlr_idle_notification_v1 *notification) {
	notification_set_idle(notification, false);
	notification_reset_timer(notification);
}

static void notification_handle_seat_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_idle_notification_v1 *notification =
		wl_container_of(listener, notification, seat_destroy);
	notification_destroy(notification);
}

static void notification_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_idle_notification_v1 *notification =
		notification_from_resource(resource);
	notification_destroy(notification);
}

static void construct_idle_notification(struct wl_client *client,
		struct wl_resource *notifier_resource, uint32_t id, uint32_t timeout,
		struct wl_resource *seat_resource, bool obey_inhibitors) {
	struct wlr_idle_notifier_v1 *notifier =
		notifier_from_resource(notifier_resource);
	struct wlr_seat_client *seat_client =
		wlr_seat_client_from_resource(seat_resource);

	uint32_t version = wl_resource_get_version(notifier_resource);
	struct wl_resource *resource = wl_resource_create(client,
		&ext_idle_notification_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &notification_impl, NULL,
		notification_handle_resource_destroy);

	if (seat_client == NULL) {
		return; // leave the resource inert
	}

	struct wlr_idle_notification_v1 *notification =
		calloc(1, sizeof(*notification));
	if (notification == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	notification->notifier = notifier;
	notification->resource = resource;
	notification->timeout_ms = timeout;
	notification->seat = seat_client->seat;
	notification->obey_inhibitors = obey_inhibitors;

	if (timeout > 0) {
		struct wl_display *display = wl_client_get_display(client);
		struct wl_event_loop *loop = wl_display_get_event_loop(display);
		notification->timer = wl_event_loop_add_timer(loop,
			notification_handle_timer, notification);
		if (notification->timer == NULL) {
			free(notification);
			wl_client_post_no_memory(client);
			return;
		}
	}

	notification->seat_destroy.notify = notification_handle_seat_destroy;
	wl_signal_add(&seat_client->seat->events.destroy, &notification->seat_destroy);

	wl_resource_set_user_data(resource, notification);
	wl_list_insert(&notifier->notifications, &notification->link);

	notification_reset_timer(notification);
}

static void notifier_handle_get_input_idle_notification(
		struct wl_client *client,
		struct wl_resource *notifier_resource, uint32_t id,
		uint32_t timeout, struct wl_resource *seat_resource) {
	construct_idle_notification(client, notifier_resource, id,
		timeout, seat_resource, false);
}

static void notifier_handle_get_idle_notification(
		struct wl_client *client,
		struct wl_resource *notifier_resource, uint32_t id,
		uint32_t timeout, struct wl_resource *seat_resource) {
	construct_idle_notification(client, notifier_resource, id,
		timeout, seat_resource, true);
}

static const struct ext_idle_notifier_v1_interface notifier_impl = {
	.destroy = resource_handle_destroy,
	.get_idle_notification = notifier_handle_get_idle_notification,
	.get_input_idle_notification = notifier_handle_get_input_idle_notification,
};

static void notifier_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_idle_notifier_v1 *notifier = data;

	struct wl_resource *resource = wl_resource_create(client,
		&ext_idle_notifier_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &notifier_impl, notifier, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_idle_notifier_v1 *notifier =
		wl_container_of(listener, notifier, display_destroy);
	wl_global_destroy(notifier->global);
	free(notifier);
}

struct wlr_idle_notifier_v1 *wlr_idle_notifier_v1_create(struct wl_display *display) {
	struct wlr_idle_notifier_v1 *notifier = calloc(1, sizeof(*notifier));
	if (notifier == NULL) {
		return NULL;
	}

	notifier->global = wl_global_create(display,
		&ext_idle_notifier_v1_interface, IDLE_NOTIFIER_VERSION, notifier,
		notifier_bind);
	if (notifier->global == NULL) {
		free(notifier);
		return NULL;
	}

	wl_list_init(&notifier->notifications);

	notifier->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &notifier->display_destroy);

	return notifier;
}

void wlr_idle_notifier_v1_set_inhibited(struct wlr_idle_notifier_v1 *notifier,
		bool inhibited) {
	if (notifier->inhibited == inhibited) {
		return;
	}

	notifier->inhibited = inhibited;

	struct wlr_idle_notification_v1 *notification;
	wl_list_for_each(notification, &notifier->notifications, link) {
		if (notification->obey_inhibitors) {
			notification_reset_timer(notification);
		}
	}
}

void wlr_idle_notifier_v1_notify_activity(struct wlr_idle_notifier_v1 *notifier,
		struct wlr_seat *seat) {
	struct wlr_idle_notification_v1 *notification;
	wl_list_for_each(notification, &notifier->notifications, link) {
		if (notification->seat == seat && !(notifier->inhibited && notification->obey_inhibitors)) {
			notification_handle_activity(notification);
		}
	}
}
