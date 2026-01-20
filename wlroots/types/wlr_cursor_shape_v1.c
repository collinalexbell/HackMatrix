#include <assert.h>
#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_tablet_tool.h>

#include "cursor-shape-v1-protocol.h"
#include "types/wlr_tablet_v2.h"

#define CURSOR_SHAPE_MANAGER_V1_VERSION 2

struct wlr_cursor_shape_device_v1 {
	struct wl_resource *resource;
	struct wlr_cursor_shape_manager_v1 *manager;
	enum wlr_cursor_shape_manager_v1_device_type type;
	struct wlr_seat_client *seat_client;
	// NULL if device_type is not TABLET_TOOL
	struct wlr_tablet_v2_tablet_tool *tablet_tool;

	struct wl_listener seat_client_destroy;
	struct wl_listener tablet_tool_destroy;
};

static const struct wp_cursor_shape_device_v1_interface device_impl;
static const struct wp_cursor_shape_manager_v1_interface manager_impl;

// Returns NULL if the resource is inert
static struct wlr_cursor_shape_device_v1 *device_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wp_cursor_shape_device_v1_interface, &device_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_cursor_shape_manager_v1 *manager_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &wp_cursor_shape_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

static void resource_handle_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void device_handle_set_shape(struct wl_client *client, struct wl_resource *device_resource,
		uint32_t serial, uint32_t shape) {
	struct wlr_cursor_shape_device_v1 *device = device_from_resource(device_resource);
	if (device == NULL) {
		return;
	}

	uint32_t version = wl_resource_get_version(device_resource);
	if (!wp_cursor_shape_device_v1_shape_is_valid(shape, version)) {
		wl_resource_post_error(device_resource, WP_CURSOR_SHAPE_DEVICE_V1_ERROR_INVALID_SHAPE,
			"Invalid shape %"PRIu32, shape);
		return;
	}

	struct wlr_cursor_shape_manager_v1_request_set_shape_event event = {
		.seat_client = device->seat_client,
		.device_type = device->type,
		.tablet_tool = device->tablet_tool,
		.serial = serial,
		.shape = shape,
	};
	wl_signal_emit_mutable(&device->manager->events.request_set_shape, &event);
}

static const struct wp_cursor_shape_device_v1_interface device_impl = {
	.destroy = resource_handle_destroy,
	.set_shape = device_handle_set_shape,
};

static void device_destroy(struct wlr_cursor_shape_device_v1 *device) {
	if (device == NULL) {
		return;
	}
	wl_list_remove(&device->seat_client_destroy.link);
	wl_list_remove(&device->tablet_tool_destroy.link);
	wl_resource_set_user_data(device->resource, NULL); // make inert
	free(device);
}

static void device_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_cursor_shape_device_v1 *device = device_from_resource(resource);
	device_destroy(device);
}

static void device_handle_seat_client_destroy(struct wl_listener *listener, void *data) {
	struct wlr_cursor_shape_device_v1 *device = wl_container_of(listener, device, seat_client_destroy);
	device_destroy(device);
}

static void device_handle_tablet_tool_destroy(struct wl_listener *listener, void *data) {
	struct wlr_cursor_shape_device_v1 *device = wl_container_of(listener, device, tablet_tool_destroy);
	device_destroy(device);
}

static void create_device(struct wl_resource *manager_resource, uint32_t id,
		struct wlr_seat_client *seat_client,
		enum wlr_cursor_shape_manager_v1_device_type type,
		struct wlr_tablet_v2_tablet_tool *tablet_tool) {
	struct wlr_cursor_shape_manager_v1 *manager = manager_from_resource(manager_resource);

	struct wl_client *client = wl_resource_get_client(manager_resource);
	uint32_t version = wl_resource_get_version(manager_resource);
	struct wl_resource *device_resource = wl_resource_create(client,
		&wp_cursor_shape_device_v1_interface, version, id);
	if (device_resource == NULL) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}
	wl_resource_set_implementation(device_resource,
		&device_impl, NULL, device_handle_resource_destroy);

	if (seat_client == NULL) {
		return; // leave the resource inert
	}

	struct wlr_cursor_shape_device_v1 *device = calloc(1, sizeof(*device));
	if (device == NULL) {
		wl_resource_post_no_memory(manager_resource);
		return;
	}

	assert((type == WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_TABLET_TOOL) ==
		(tablet_tool != NULL));

	device->resource = device_resource;
	device->manager = manager;
	device->type = type;
	device->tablet_tool = tablet_tool;
	device->seat_client = seat_client;

	device->seat_client_destroy.notify = device_handle_seat_client_destroy;
	wl_signal_add(&seat_client->events.destroy, &device->seat_client_destroy);

	if (tablet_tool != NULL) {
	    device->tablet_tool_destroy.notify = device_handle_tablet_tool_destroy;
	    wl_signal_add(&tablet_tool->wlr_tool->events.destroy, &device->tablet_tool_destroy);
	} else {
	    wl_list_init(&device->tablet_tool_destroy.link);
	}

	wl_resource_set_user_data(device_resource, device);
}

static void manager_handle_get_pointer(struct wl_client *client, struct wl_resource *manager_resource,
		uint32_t id, struct wl_resource *pointer_resource) {
	struct wlr_seat_client *seat_client = wlr_seat_client_from_pointer_resource(pointer_resource);
	create_device(manager_resource, id, seat_client,
		WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_POINTER, NULL);
}

static void manager_handle_get_tablet_tool_v2(struct wl_client *client, struct wl_resource *manager_resource,
		uint32_t id, struct wl_resource *tablet_tool_resource) {
	struct wlr_tablet_tool_client_v2 *tablet_tool_client = tablet_tool_client_from_resource(tablet_tool_resource);

	struct wlr_seat_client *seat_client = NULL;
	struct wlr_tablet_v2_tablet_tool *tablet_tool = NULL;
	if (tablet_tool_client != NULL && tablet_tool_client->tool != NULL) {
		seat_client = tablet_tool_client->seat->seat_client;
		tablet_tool = tablet_tool_client->tool;
	}

	create_device(manager_resource, id, seat_client,
		WLR_CURSOR_SHAPE_MANAGER_V1_DEVICE_TYPE_TABLET_TOOL, tablet_tool);
}

static const struct wp_cursor_shape_manager_v1_interface manager_impl = {
	.destroy = resource_handle_destroy,
	.get_pointer = manager_handle_get_pointer,
	.get_tablet_tool_v2 = manager_handle_get_tablet_tool_v2,
};

static void manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_cursor_shape_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&wp_cursor_shape_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, manager, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_cursor_shape_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);

	wl_signal_emit_mutable(&manager->events.destroy, NULL);

	assert(wl_list_empty(&manager->events.request_set_shape.listener_list));
	assert(wl_list_empty(&manager->events.destroy.listener_list));

	wl_global_destroy(manager->global);
	wl_list_remove(&manager->display_destroy.link);
	free(manager);
}

struct wlr_cursor_shape_manager_v1 *wlr_cursor_shape_manager_v1_create(
		struct wl_display *display, uint32_t version) {
	assert(version <= CURSOR_SHAPE_MANAGER_V1_VERSION);

	struct wlr_cursor_shape_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&wp_cursor_shape_manager_v1_interface, version, manager, manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.request_set_shape);
	wl_signal_init(&manager->events.destroy);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}

static const char *const shape_names[] = {
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT] = "default",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CONTEXT_MENU] = "context-menu",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_HELP] = "help",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_POINTER] = "pointer",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_PROGRESS] = "progress",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_WAIT] = "wait",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CELL] = "cell",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR] = "crosshair",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT] = "text",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_VERTICAL_TEXT] = "vertical-text",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALIAS] = "alias",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COPY] = "copy",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_MOVE] = "move",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NO_DROP] = "no-drop",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NOT_ALLOWED] = "not-allowed",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRAB] = "grab",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_GRABBING] = "grabbing",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_E_RESIZE] = "e-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_N_RESIZE] = "n-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NE_RESIZE] = "ne-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NW_RESIZE] = "nw-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_S_RESIZE] = "s-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SE_RESIZE] = "se-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_SW_RESIZE] = "sw-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_W_RESIZE] = "w-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE] = "ew-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE] = "ns-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE] = "nesw-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE] = "nwse-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_COL_RESIZE] = "col-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ROW_RESIZE] = "row-resize",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL] = "all-scroll",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_IN] = "zoom-in",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ZOOM_OUT] = "zoom-out",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DND_ASK] = "dnd-ask",
	[WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_RESIZE] = "all-resize",
};

const char *wlr_cursor_shape_v1_name(enum wp_cursor_shape_device_v1_shape shape) {
	assert(shape < sizeof(shape_names) / sizeof(shape_names[0]));
	return shape_names[shape];
}
