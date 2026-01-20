#include <assert.h>
#include <stdlib.h>
#include <wayland-util.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_transient_seat_v1.h>
#include "ext-transient-seat-v1-protocol.h"

static const struct ext_transient_seat_manager_v1_interface manager_impl;
static const struct ext_transient_seat_v1_interface transient_seat_impl;

static struct wlr_transient_seat_manager_v1 *manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_transient_seat_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_transient_seat_v1 *transient_seat_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_transient_seat_v1_interface, &transient_seat_impl));
	return wl_resource_get_user_data(resource);
}

static void transient_seat_destroy(struct wlr_transient_seat_v1 *seat) {
	wl_list_remove(&seat->seat_destroy.link);
	wlr_seat_destroy(seat->seat);
	free(seat);
}

static void transient_seat_handle_resource_destroy(
		struct wl_resource *resource) {
	struct wlr_transient_seat_v1 *seat =
		transient_seat_from_resource(resource);
	transient_seat_destroy(seat);
}

static void transient_seat_handle_destroy(struct wl_client *client,
		struct wl_resource *seat_resource) {
	wl_resource_destroy(seat_resource);
}

static const struct ext_transient_seat_v1_interface transient_seat_impl = {
	.destroy = transient_seat_handle_destroy,
};

static void manager_create_transient_seat(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id) {
	struct wlr_transient_seat_manager_v1 *manager =
		manager_from_resource(manager_resource);

	struct wlr_transient_seat_v1 *seat = calloc(1, sizeof(*seat));
	if (!seat) {
		goto failure;
	}

	int version = wl_resource_get_version(manager_resource);
	seat->resource = wl_resource_create(client,
			&ext_transient_seat_v1_interface, version, id);
	if (!seat->resource) {
		goto failure;
	}

	wl_resource_set_implementation(seat->resource, &transient_seat_impl,
			seat, transient_seat_handle_resource_destroy);

	wl_list_init(&seat->seat_destroy.link);
	wl_signal_emit_mutable(&manager->events.create_seat, seat);

	return;

failure:
	free(seat);
	wl_client_post_no_memory(client);
}

static void transient_seat_handle_seat_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_transient_seat_v1 *seat = wl_container_of(listener, seat,
			seat_destroy);
	seat->seat = NULL;
	wl_resource_set_user_data(seat->resource, NULL);
	transient_seat_destroy(seat);
}

void wlr_transient_seat_v1_ready(struct wlr_transient_seat_v1 *seat,
		struct wlr_seat *wlr_seat) {
	assert(wlr_seat);

	seat->seat = wlr_seat;

	seat->seat_destroy.notify = transient_seat_handle_seat_destroy;
	wl_signal_add(&wlr_seat->events.destroy, &seat->seat_destroy);

	struct wl_client *client = wl_resource_get_client(seat->resource);
	uint32_t global_name = wl_global_get_name(seat->seat->global, client);
	assert(global_name != 0);
	ext_transient_seat_v1_send_ready(seat->resource, global_name);
}

void wlr_transient_seat_v1_deny(struct wlr_transient_seat_v1 *seat) {
	ext_transient_seat_v1_send_denied(seat->resource);
}

static void manager_handle_destroy(struct wl_client *client,
		struct wl_resource *manager_resource) {
	wl_resource_destroy(manager_resource);
}

static const struct ext_transient_seat_manager_v1_interface manager_impl = {
	.create = manager_create_transient_seat,
	.destroy = manager_handle_destroy,
};

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_transient_seat_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_signal_emit_mutable(&manager->events.destroy, NULL);

	assert(wl_list_empty(&manager->events.destroy.listener_list));
	assert(wl_list_empty(&manager->events.create_seat.listener_list));

	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

static void transient_seat_manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_transient_seat_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&ext_transient_seat_manager_v1_interface, version, id);

	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, &manager_impl, manager, NULL);
}

struct wlr_transient_seat_manager_v1 *wlr_transient_seat_manager_v1_create(
		struct wl_display *display) {
	struct wlr_transient_seat_manager_v1 *manager =
		calloc(1, sizeof(*manager));
	if (!manager) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&ext_transient_seat_manager_v1_interface, 1, manager,
		transient_seat_manager_bind);
	if (!manager->global) {
		free(manager);
		return NULL;
	}

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	wl_signal_init(&manager->events.destroy);
	wl_signal_init(&manager->events.create_seat);

	return manager;
}
