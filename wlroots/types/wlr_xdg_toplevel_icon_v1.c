#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <wayland-server-protocol.h>

#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_xdg_toplevel_icon_v1.h>
#include <wlr/util/log.h>

#include "xdg-toplevel-icon-v1-protocol.h"

#define MANAGER_VERSION 1

static const struct xdg_toplevel_icon_v1_interface icon_impl;

static struct wlr_xdg_toplevel_icon_v1 *icon_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &xdg_toplevel_icon_v1_interface, &icon_impl));
	return wl_resource_get_user_data(resource);
}

static void icon_destroy(struct wlr_xdg_toplevel_icon_v1 *icon) {
	struct wlr_xdg_toplevel_icon_v1_buffer *icon_buffer, *tmp;
	wl_list_for_each_safe(icon_buffer, tmp, &icon->buffers, link) {
		wlr_buffer_unlock(icon_buffer->buffer);
		wl_list_remove(&icon_buffer->link);
		free(icon_buffer);
	}

	free(icon->name);
	free(icon);
}

static void icon_handle_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void icon_handle_set_name(struct wl_client *client, struct wl_resource *resource,
		const char *name) {
	struct wlr_xdg_toplevel_icon_v1 *icon = icon_from_resource(resource);
	if (icon->immutable) {
		wl_resource_post_error(resource, XDG_TOPLEVEL_ICON_V1_ERROR_IMMUTABLE,
			"the icon has already been assigned to a toplevel and must not be changed");
		return;
	}

	char *dup = strdup(name);
	if (dup == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	free(icon->name);
	icon->name = dup;
}

static void icon_handle_add_buffer(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *buffer_resource, int32_t scale) {
	struct wlr_xdg_toplevel_icon_v1 *icon = icon_from_resource(resource);
	if (icon->immutable) {
		wl_resource_post_error(resource, XDG_TOPLEVEL_ICON_V1_ERROR_IMMUTABLE,
			"the icon has already been assigned to a toplevel and must not be changed");
		return;
	}

	struct wlr_buffer *buffer = wlr_buffer_try_from_resource(buffer_resource);

	const char *bad_buffer_msg = NULL;

	struct wlr_shm_attributes shm_attribs;
	if (!wlr_buffer_get_shm(buffer, &shm_attribs)) {
		bad_buffer_msg = "not backed by wl_shm";
	} else if (buffer->width != buffer->height) {
		bad_buffer_msg = "not square";
	}

	if (bad_buffer_msg != NULL) {
		wl_resource_post_error(resource, XDG_TOPLEVEL_ICON_V1_ERROR_INVALID_BUFFER,
			"the provided buffer does not satisfy requirements: %s", bad_buffer_msg);
		wlr_buffer_unlock(buffer);
		return;
	}

	struct wlr_xdg_toplevel_icon_v1_buffer *icon_buffer;
	wl_list_for_each(icon_buffer, &icon->buffers, link) {
		if (icon_buffer->buffer->width == buffer->width && icon_buffer->scale == scale) {
			wlr_buffer_unlock(icon_buffer->buffer);
			icon_buffer->buffer = buffer;
			return;
		}
	}

	icon_buffer = calloc(1, sizeof(*icon_buffer));
	if (icon_buffer == NULL) {
		wl_resource_post_no_memory(resource);
	}

	icon_buffer->buffer = buffer;
	icon_buffer->scale = scale;

	wl_list_insert(&icon->buffers, &icon_buffer->link);
}

static const struct xdg_toplevel_icon_v1_interface icon_impl = {
	.destroy = icon_handle_destroy,
	.set_name = icon_handle_set_name,
	.add_buffer = icon_handle_add_buffer,
};

static void icon_handle_resource_destroy(struct wl_resource *resource) {
	wlr_xdg_toplevel_icon_v1_unref(icon_from_resource(resource));
}

struct wlr_xdg_toplevel_icon_v1 *wlr_xdg_toplevel_icon_v1_ref(
		struct wlr_xdg_toplevel_icon_v1 *icon) {
	++icon->n_refs;
	return icon;
}

void wlr_xdg_toplevel_icon_v1_unref(struct wlr_xdg_toplevel_icon_v1 *icon) {
	if (icon == NULL) {
		return;
	}

	assert(icon->n_refs > 0);
	--icon->n_refs;
	if (icon->n_refs == 0) {
		icon_destroy(icon);
	};
}

static const struct xdg_toplevel_icon_manager_v1_interface manager_impl;

static struct wlr_xdg_toplevel_icon_manager_v1 *manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &xdg_toplevel_icon_manager_v1_interface,
		&manager_impl));
	return wl_resource_get_user_data(resource);
}

static void manager_handle_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void manager_handle_create_icon(struct wl_client *client, struct wl_resource *resource,
		uint32_t id) {
	struct wlr_xdg_toplevel_icon_v1 *icon = calloc(1, sizeof(*icon));
	if (icon == NULL) {
		wl_client_post_no_memory(client);
		return;
	}

	struct wl_resource *icon_resource = wl_resource_create(client,
		&xdg_toplevel_icon_v1_interface, wl_resource_get_version(resource), id);
	if (icon_resource == NULL) {
		wl_client_post_no_memory(client);
		free(icon);
		return;
	}

	wl_list_init(&icon->buffers);
	icon->n_refs = 1;

	wl_resource_set_implementation(icon_resource, &icon_impl, icon, icon_handle_resource_destroy);
}

static void manager_handle_set_icon(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *toplevel_resource, struct wl_resource *icon_resource) {
	struct wlr_xdg_toplevel_icon_manager_v1 *manager = manager_from_resource(resource);
	struct wlr_xdg_toplevel *toplevel = wlr_xdg_toplevel_from_resource(toplevel_resource);

	struct wlr_xdg_toplevel_icon_v1 *icon = NULL;
	if (icon_resource != NULL) {
		icon = icon_from_resource(icon_resource);
		icon->immutable = true;

		if (icon->name == NULL && wl_list_empty(&icon->buffers)) {
			// Same as supplying null icon
			icon = NULL;
		}
	}

	struct wlr_xdg_toplevel_icon_manager_v1_set_icon_event event = {
		.toplevel = toplevel,
		.icon = icon,
	};

	wl_signal_emit_mutable(&manager->events.set_icon, &event);
}

static const struct xdg_toplevel_icon_manager_v1_interface manager_impl = {
	.destroy = manager_handle_destroy,
	.create_icon = manager_handle_create_icon,
	.set_icon = manager_handle_set_icon,
};

static void manager_send_sizes(struct wlr_xdg_toplevel_icon_manager_v1 *manager,
		struct wl_resource *resource) {
	for (size_t i = 0; i < manager->n_sizes; i++) {
		xdg_toplevel_icon_manager_v1_send_icon_size(resource, manager->sizes[i]);
	}
	xdg_toplevel_icon_manager_v1_send_done(resource);
}

static void manager_handle_resource_destroy(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static void manager_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wlr_xdg_toplevel_icon_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&xdg_toplevel_icon_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, manager, manager_handle_resource_destroy);

	wl_list_insert(&manager->resources, wl_resource_get_link(resource));

	manager_send_sizes(manager, resource);
}

static void manager_handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel_icon_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);

	wl_signal_emit_mutable(&manager->events.destroy, NULL);

	assert(wl_list_empty(&manager->events.set_icon.listener_list));
	assert(wl_list_empty(&manager->events.destroy.listener_list));

	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);

	wl_list_remove(&manager->resources);

	free(manager->sizes);
	free(manager);
}

void wlr_xdg_toplevel_icon_manager_v1_set_sizes(struct wlr_xdg_toplevel_icon_manager_v1 *manager,
		int *sizes, size_t n_sizes) {
	if (n_sizes != manager->n_sizes) {
		int *dup_sizes = NULL;
		if (n_sizes > 0) {
			dup_sizes = calloc(n_sizes, sizeof(*dup_sizes));
			if (dup_sizes == NULL) {
				wlr_log(WLR_ERROR, "Allocation failed");
				return;
			}
		}

		free(manager->sizes);
		manager->sizes = dup_sizes;
		manager->n_sizes = n_sizes;
	}

	for (size_t i = 0; i < n_sizes; i++) {
		manager->sizes[i] = sizes[i];
	}

	struct wl_resource *resource;
	wl_resource_for_each(resource, &manager->resources) {
		manager_send_sizes(manager, resource);
	}
}

struct wlr_xdg_toplevel_icon_manager_v1 *wlr_xdg_toplevel_icon_manager_v1_create(
			struct wl_display *display, uint32_t version) {
	assert(version <= MANAGER_VERSION);

	struct wlr_xdg_toplevel_icon_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->global = wl_global_create(display, &xdg_toplevel_icon_manager_v1_interface,
		version, manager, manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.set_icon);
	wl_signal_init(&manager->events.destroy);

	wl_list_init(&manager->resources);

	manager->display_destroy.notify = manager_handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}
