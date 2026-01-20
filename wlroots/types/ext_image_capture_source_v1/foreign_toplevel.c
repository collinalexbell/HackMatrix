#include <assert.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include "ext-image-capture-source-v1-protocol.h"

#define FOREIGN_TOPLEVEL_IMAGE_SOURCE_MANAGER_V1_VERSION 1

static const struct ext_foreign_toplevel_image_capture_source_manager_v1_interface foreign_toplevel_manager_impl;

static struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1 *
foreign_toplevel_manager_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_foreign_toplevel_image_capture_source_manager_v1_interface,
		&foreign_toplevel_manager_impl));
	return wl_resource_get_user_data(resource);
}

bool wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request_accept(
		struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request,
		struct wlr_ext_image_capture_source_v1 *source) {
	return wlr_ext_image_capture_source_v1_create_resource(source, request->client, request->new_id);
}

static void foreign_toplevel_manager_handle_create_source(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t new_id,
		struct wl_resource *foreign_toplevel_resource) {
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1 *manager =
		foreign_toplevel_manager_from_resource(manager_resource);
	struct wlr_ext_foreign_toplevel_handle_v1 *toplevel_handle =
		wlr_ext_foreign_toplevel_handle_v1_from_resource(foreign_toplevel_resource);
	if (toplevel_handle == NULL) {
		wlr_ext_image_capture_source_v1_create_resource(NULL, client, new_id);
		return;
	}

	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1_request *request =
		calloc(1, sizeof(*request));
	if (request == NULL) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	request->toplevel_handle = toplevel_handle;
	request->client = client;
	request->new_id = new_id;

	wl_signal_emit_mutable(&manager->events.new_request, request);
}

static void foreign_toplevel_manager_handle_destroy(struct wl_client *client,
		struct wl_resource *manager_resource) {
	wl_resource_destroy(manager_resource);
}

static const struct ext_foreign_toplevel_image_capture_source_manager_v1_interface foreign_toplevel_manager_impl = {
	.create_source = foreign_toplevel_manager_handle_create_source,
	.destroy = foreign_toplevel_manager_handle_destroy,
};

static void foreign_toplevel_manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&ext_foreign_toplevel_image_capture_source_manager_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &foreign_toplevel_manager_impl, manager, NULL);
}

static void foreign_toplevel_manager_handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_signal_emit_mutable(&manager->events.destroy, NULL);
	assert(wl_list_empty(&manager->events.destroy.listener_list));
	assert(wl_list_empty(&manager->events.new_request.listener_list));
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1 *
wlr_ext_foreign_toplevel_image_capture_source_manager_v1_create(struct wl_display *display,
		uint32_t version) {
	assert(version <= FOREIGN_TOPLEVEL_IMAGE_SOURCE_MANAGER_V1_VERSION);

	struct wlr_ext_foreign_toplevel_image_capture_source_manager_v1 *manager =
		calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&ext_foreign_toplevel_image_capture_source_manager_v1_interface,
		version, manager, foreign_toplevel_manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.destroy);
	wl_signal_init(&manager->events.new_request);

	manager->display_destroy.notify = foreign_toplevel_manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}
