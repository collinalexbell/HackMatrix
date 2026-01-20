#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_ext_foreign_toplevel_list_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include "ext-foreign-toplevel-list-v1-protocol.h"

#include "util/token.h"

#define FOREIGN_TOPLEVEL_LIST_V1_VERSION 1

static const struct ext_foreign_toplevel_handle_v1_interface toplevel_handle_impl;

static void foreign_toplevel_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_foreign_toplevel_handle_v1_interface,
		&toplevel_handle_impl));

	wl_resource_destroy(resource);
}

static const struct ext_foreign_toplevel_handle_v1_interface toplevel_handle_impl = {
	.destroy = foreign_toplevel_handle_destroy,
};

// Returns true if clients need to be notified about the update
static bool update_string(struct wlr_ext_foreign_toplevel_handle_v1 *toplevel,
		char **dst, const char *src) {
	if (src == NULL) {
		if (*dst == NULL) {
			return false;
		}
	} else if (*dst != NULL && strcmp(*dst, src) == 0) {
		return false;
	}

	free(*dst);
	if (src != NULL) {
		*dst = strdup(src);
		if (*dst == NULL) {
			struct wl_resource *resource;
			wl_resource_for_each(resource, &toplevel->resources) {
				wl_resource_post_no_memory(resource);
			}
			return false;
		}
	} else {
		*dst = NULL;
	}
	return true;
}

struct wlr_ext_foreign_toplevel_handle_v1 *wlr_ext_foreign_toplevel_handle_v1_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &ext_foreign_toplevel_handle_v1_interface,
		&toplevel_handle_impl));
	return wl_resource_get_user_data(resource);
}

void wlr_ext_foreign_toplevel_handle_v1_update_state(
		struct wlr_ext_foreign_toplevel_handle_v1 *toplevel,
		const struct wlr_ext_foreign_toplevel_handle_v1_state *state) {
	bool changed_app_id = update_string(toplevel, &toplevel->app_id, state->app_id);
	bool changed_title = update_string(toplevel, &toplevel->title, state->title);

	if (!changed_app_id && !changed_title) {
		return;
	}

	struct wl_resource *resource;
	wl_resource_for_each(resource, &toplevel->resources) {
		if (changed_app_id) {
			ext_foreign_toplevel_handle_v1_send_app_id(resource, state->app_id ? state->app_id : "");
		}
		if (changed_title) {
			ext_foreign_toplevel_handle_v1_send_title(resource, state->title ? state->title : "");
		}
		ext_foreign_toplevel_handle_v1_send_done(resource);
	}
}

void wlr_ext_foreign_toplevel_handle_v1_destroy(
		struct wlr_ext_foreign_toplevel_handle_v1 *toplevel) {
	if (!toplevel) {
		return;
	}

	wl_signal_emit_mutable(&toplevel->events.destroy, NULL);

	assert(wl_list_empty(&toplevel->events.destroy.listener_list));

	struct wl_resource *resource, *tmp;
	wl_resource_for_each_safe(resource, tmp, &toplevel->resources) {
		ext_foreign_toplevel_handle_v1_send_closed(resource);
		wl_resource_set_user_data(resource, NULL);
		wl_list_remove(wl_resource_get_link(resource));
		wl_list_init(wl_resource_get_link(resource));
	}

	wl_list_remove(&toplevel->link);

	free(toplevel->title);
	free(toplevel->app_id);
	free(toplevel->identifier);
	free(toplevel);
}

static void foreign_toplevel_resource_destroy(struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static struct wl_resource *create_toplevel_resource_for_resource(
		struct wlr_ext_foreign_toplevel_handle_v1 *toplevel,
		struct wl_resource *list_resource) {
	struct wl_client *client = wl_resource_get_client(list_resource);
	struct wl_resource *resource = wl_resource_create(client,
			&ext_foreign_toplevel_handle_v1_interface,
			wl_resource_get_version(list_resource), 0);
	if (!resource) {
		wl_client_post_no_memory(client);
		return NULL;
	}

	wl_resource_set_implementation(resource, &toplevel_handle_impl, toplevel,
		foreign_toplevel_resource_destroy);

	wl_list_insert(&toplevel->resources, wl_resource_get_link(resource));
	ext_foreign_toplevel_list_v1_send_toplevel(list_resource, resource);
	return resource;
}

static void toplevel_send_details_to_toplevel_resource(
		struct wlr_ext_foreign_toplevel_handle_v1 *toplevel,
		struct wl_resource *resource) {
	if (toplevel->title) {
		ext_foreign_toplevel_handle_v1_send_title(resource, toplevel->title);
	}
	if (toplevel->app_id) {
		ext_foreign_toplevel_handle_v1_send_app_id(resource, toplevel->app_id);
	}
	assert(toplevel->identifier); // Required to exist by protocol.
	ext_foreign_toplevel_handle_v1_send_identifier(resource, toplevel->identifier);
	ext_foreign_toplevel_handle_v1_send_done(resource);
}

struct wlr_ext_foreign_toplevel_handle_v1 *
wlr_ext_foreign_toplevel_handle_v1_create(struct wlr_ext_foreign_toplevel_list_v1 *list,
		const struct wlr_ext_foreign_toplevel_handle_v1_state *state) {
	struct wlr_ext_foreign_toplevel_handle_v1 *toplevel = calloc(1, sizeof(*toplevel));
	if (!toplevel) {
		wlr_log(WLR_ERROR, "failed to allocate memory for toplevel handle");
		return NULL;
	}

	toplevel->identifier = calloc(TOKEN_SIZE, sizeof(char));
	if (toplevel->identifier == NULL) {
		wlr_log(WLR_ERROR, "failed to allocate memory for toplevel identifier");
		free(toplevel);
		return NULL;
	}

	if (!generate_token(toplevel->identifier)) {
		free(toplevel->identifier);
		free(toplevel);
		return NULL;
	}

	wl_list_insert(&list->toplevels, &toplevel->link);
	toplevel->list = list;
	if (state->app_id) {
		toplevel->app_id = strdup(state->app_id);
	}
	if (state->title) {
		toplevel->title = strdup(state->title);
	}

	wl_list_init(&toplevel->resources);

	wl_signal_init(&toplevel->events.destroy);

	struct wl_resource *list_resource, *toplevel_resource;
	wl_resource_for_each(list_resource, &list->resources) {
		toplevel_resource = create_toplevel_resource_for_resource(toplevel, list_resource);
		if (!toplevel_resource) {
			continue;
		}
		toplevel_send_details_to_toplevel_resource(toplevel, toplevel_resource);
	}

	return toplevel;
}

static const struct ext_foreign_toplevel_list_v1_interface
	foreign_toplevel_list_impl;

static void foreign_toplevel_list_handle_stop(struct wl_client *client,
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_foreign_toplevel_list_v1_interface,
		&foreign_toplevel_list_impl));

	ext_foreign_toplevel_list_v1_send_finished(resource);
	wl_list_remove(wl_resource_get_link(resource));
	wl_list_init(wl_resource_get_link(resource));
}

static void foreign_toplevel_list_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&ext_foreign_toplevel_list_v1_interface,
		&foreign_toplevel_list_impl));

	wl_resource_destroy(resource);
}

static const struct ext_foreign_toplevel_list_v1_interface
		foreign_toplevel_list_impl = {
	.stop = foreign_toplevel_list_handle_stop,
	.destroy = foreign_toplevel_list_handle_destroy
};

static void foreign_toplevel_list_resource_destroy(
		struct wl_resource *resource) {
	wl_list_remove(wl_resource_get_link(resource));
}

static void foreign_toplevel_list_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_ext_foreign_toplevel_list_v1 *list = data;
	struct wl_resource *resource = wl_resource_create(client,
			&ext_foreign_toplevel_list_v1_interface, version, id);
	if (!resource) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &foreign_toplevel_list_impl,
			list, foreign_toplevel_list_resource_destroy);

	wl_list_insert(&list->resources, wl_resource_get_link(resource));

	struct wlr_ext_foreign_toplevel_handle_v1 *toplevel;
	wl_list_for_each(toplevel, &list->toplevels, link) {
		struct wl_resource *toplevel_resource =
			create_toplevel_resource_for_resource(toplevel, resource);
		toplevel_send_details_to_toplevel_resource(toplevel,
			toplevel_resource);
	}
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_ext_foreign_toplevel_list_v1 *list =
		wl_container_of(listener, list, display_destroy);
	wl_signal_emit_mutable(&list->events.destroy, NULL);

	assert(wl_list_empty(&list->events.destroy.listener_list));

	wl_list_remove(&list->display_destroy.link);
	wl_global_destroy(list->global);
	free(list);
}

struct wlr_ext_foreign_toplevel_list_v1 *wlr_ext_foreign_toplevel_list_v1_create(
		struct wl_display *display, uint32_t version) {
	assert(version <= FOREIGN_TOPLEVEL_LIST_V1_VERSION);

	struct wlr_ext_foreign_toplevel_list_v1 *list = calloc(1, sizeof(*list));
	if (!list) {
		return NULL;
	}

	list->global = wl_global_create(display,
			&ext_foreign_toplevel_list_v1_interface,
			version, list,
			foreign_toplevel_list_bind);
	if (!list->global) {
		free(list);
		return NULL;
	}

	wl_signal_init(&list->events.destroy);

	wl_list_init(&list->resources);
	wl_list_init(&list->toplevels);

	list->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &list->display_destroy);

	return list;
}
