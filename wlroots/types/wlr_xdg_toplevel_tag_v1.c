#include <assert.h>
#include <stdlib.h>

#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_toplevel_tag_v1.h>

#include "xdg-toplevel-tag-v1-protocol.h"

#define MANAGER_VERSION 1

static const struct xdg_toplevel_tag_manager_v1_interface manager_impl;

static struct wlr_xdg_toplevel_tag_manager_v1 *manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &xdg_toplevel_tag_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

static void manager_handle_set_tag(struct wl_client *client, struct wl_resource *manager_resource,
		struct wl_resource *toplevel_resource, const char *tag) {
	struct wlr_xdg_toplevel_tag_manager_v1 *manager = manager_from_resource(manager_resource);
	struct wlr_xdg_toplevel *toplevel = wlr_xdg_toplevel_from_resource(toplevel_resource);

	struct wlr_xdg_toplevel_tag_manager_v1_set_tag_event event = {
		.toplevel = toplevel,
		.tag = tag,
	};
	wl_signal_emit_mutable(&manager->events.set_tag, &event);
}

static void manager_handle_set_description(struct wl_client *client, struct wl_resource *manager_resource,
		struct wl_resource *toplevel_resource, const char *description) {
	struct wlr_xdg_toplevel_tag_manager_v1 *manager = manager_from_resource(manager_resource);
	struct wlr_xdg_toplevel *toplevel = wlr_xdg_toplevel_from_resource(toplevel_resource);

	struct wlr_xdg_toplevel_tag_manager_v1_set_description_event event = {
		.toplevel = toplevel,
		.description = description,
	};
	wl_signal_emit_mutable(&manager->events.set_description, &event);
}

static void manager_handle_destroy(struct wl_client *client, struct wl_resource *manager_resource) {
	wl_resource_destroy(manager_resource);
}

static const struct xdg_toplevel_tag_manager_v1_interface manager_impl = {
	.destroy = manager_handle_destroy,
	.set_toplevel_tag = manager_handle_set_tag,
	.set_toplevel_description = manager_handle_set_description,
};

static void manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wlr_xdg_toplevel_tag_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&xdg_toplevel_tag_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, manager, NULL);
}

static void manager_handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_tag_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);

	wl_signal_emit_mutable(&manager->events.destroy, NULL);

	assert(wl_list_empty(&manager->events.set_tag.listener_list));
	assert(wl_list_empty(&manager->events.set_description.listener_list));
	assert(wl_list_empty(&manager->events.destroy.listener_list));

	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wlr_xdg_toplevel_tag_manager_v1 *wlr_xdg_toplevel_tag_manager_v1_create(
		struct wl_display *display, uint32_t version) {
	assert(version <= MANAGER_VERSION);

	struct wlr_xdg_toplevel_tag_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display, &xdg_toplevel_tag_manager_v1_interface,
		version, manager, manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.set_tag);
	wl_signal_init(&manager->events.set_description);
	wl_signal_init(&manager->events.destroy);

	manager->display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}
