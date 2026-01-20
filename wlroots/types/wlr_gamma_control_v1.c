#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/color.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include "wlr-gamma-control-unstable-v1-protocol.h"

#define GAMMA_CONTROL_MANAGER_V1_VERSION 1

static void gamma_control_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void gamma_control_destroy(struct wlr_gamma_control_v1 *gamma_control) {
	if (gamma_control == NULL) {
		return;
	}

	struct wlr_gamma_control_manager_v1 *manager = gamma_control->manager;
	struct wlr_output *output = gamma_control->output;

	wl_resource_set_user_data(gamma_control->resource, NULL);
	wl_list_remove(&gamma_control->output_destroy_listener.link);
	wl_list_remove(&gamma_control->link);
	free(gamma_control->table);
	free(gamma_control);

	struct wlr_gamma_control_manager_v1_set_gamma_event event = {
		.output = output,
	};
	wl_signal_emit_mutable(&manager->events.set_gamma, &event);
}

static const struct zwlr_gamma_control_v1_interface gamma_control_impl;

static struct wlr_gamma_control_v1 *gamma_control_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &zwlr_gamma_control_v1_interface,
		&gamma_control_impl));
	return wl_resource_get_user_data(resource);
}

static void gamma_control_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_gamma_control_v1 *gamma_control =
		gamma_control_from_resource(resource);
	gamma_control_destroy(gamma_control);
}

static void gamma_control_handle_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_gamma_control_v1 *gamma_control =
		wl_container_of(listener, gamma_control, output_destroy_listener);
	gamma_control_destroy(gamma_control);
}

static void gamma_control_handle_set_gamma(struct wl_client *client,
		struct wl_resource *gamma_control_resource, int fd) {
	struct wlr_gamma_control_v1 *gamma_control =
		gamma_control_from_resource(gamma_control_resource);
	if (gamma_control == NULL) {
		goto error_fd;
	}

	size_t table_size = gamma_control->ramp_size * 3 * sizeof(uint16_t);

	// Refuse to block when reading
	int fd_flags = fcntl(fd, F_GETFL, 0);
	if (fd_flags == -1) {
		wlr_log_errno(WLR_ERROR, "failed to get FD flags");
		wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
		goto error_fd;
	}
	if (fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK) == -1) {
		wlr_log_errno(WLR_ERROR, "failed to set FD flags");
		wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
		goto error_fd;
	}

	// Use the heap since gamma tables can be large
	uint16_t *table = malloc(table_size);
	if (table == NULL) {
		wl_resource_post_no_memory(gamma_control_resource);
		goto error_fd;
	}

	ssize_t n_read = pread(fd, table, table_size, 0);
	if (n_read < 0) {
		wlr_log_errno(WLR_ERROR, "failed to read gamma table");
		wlr_gamma_control_v1_send_failed_and_destroy(gamma_control);
		goto error_table;
	} else if ((size_t)n_read != table_size) {
		wl_resource_post_error(gamma_control_resource,
			ZWLR_GAMMA_CONTROL_V1_ERROR_INVALID_GAMMA,
			"The gamma ramps don't have the correct size");
		goto error_table;
	}
	close(fd);

	free(gamma_control->table);
	gamma_control->table = table;

	struct wlr_gamma_control_manager_v1_set_gamma_event event = {
		.output = gamma_control->output,
		.control = gamma_control,
	};
	wl_signal_emit_mutable(&gamma_control->manager->events.set_gamma, &event);

	return;

error_table:
	free(table);
error_fd:
	close(fd);
}

static const struct zwlr_gamma_control_v1_interface gamma_control_impl = {
	.destroy = gamma_control_handle_destroy,
	.set_gamma = gamma_control_handle_set_gamma,
};

static const struct zwlr_gamma_control_manager_v1_interface
	gamma_control_manager_impl;

static struct wlr_gamma_control_manager_v1 *gamma_control_manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&zwlr_gamma_control_manager_v1_interface, &gamma_control_manager_impl));
	return wl_resource_get_user_data(resource);
}

static void gamma_control_manager_get_gamma_control(struct wl_client *client,
		struct wl_resource *manager_resource, uint32_t id,
		struct wl_resource *output_resource) {
	struct wlr_gamma_control_manager_v1 *manager =
		gamma_control_manager_from_resource(manager_resource);
	struct wlr_output *output = wlr_output_from_resource(output_resource);

	uint32_t version = wl_resource_get_version(manager_resource);
	struct wl_resource *resource = wl_resource_create(client,
		&zwlr_gamma_control_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &gamma_control_impl,
		NULL, gamma_control_handle_resource_destroy);

	if (output == NULL) {
		zwlr_gamma_control_v1_send_failed(resource);
		return;
	}

	size_t gamma_size = wlr_output_get_gamma_size(output);
	if (gamma_size == 0) {
		gamma_size = manager->fallback_gamma_size;
	}
	if (gamma_size == 0) {
		zwlr_gamma_control_v1_send_failed(resource);
		return;
	}

	if (wlr_gamma_control_manager_v1_get_control(manager, output) != NULL) {
		zwlr_gamma_control_v1_send_failed(resource);
		return;
	}

	struct wlr_gamma_control_v1 *gamma_control = calloc(1, sizeof(*gamma_control));
	if (gamma_control == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	gamma_control->output = output;
	gamma_control->manager = manager;
	gamma_control->resource = resource;
	gamma_control->ramp_size = gamma_size;
	wl_resource_set_user_data(resource, gamma_control);

	wl_signal_add(&output->events.destroy,
		&gamma_control->output_destroy_listener);
	gamma_control->output_destroy_listener.notify =
		gamma_control_handle_output_destroy;

	wl_list_insert(&manager->controls, &gamma_control->link);
	zwlr_gamma_control_v1_send_gamma_size(gamma_control->resource, gamma_size);
}

static void gamma_control_manager_destroy(struct wl_client *client,
		struct wl_resource *manager_resource) {
	wl_resource_destroy(manager_resource);
}

static const struct zwlr_gamma_control_manager_v1_interface
		gamma_control_manager_impl = {
	.get_gamma_control = gamma_control_manager_get_gamma_control,
	.destroy = gamma_control_manager_destroy,
};

static void gamma_control_manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_gamma_control_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&zwlr_gamma_control_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &gamma_control_manager_impl,
		manager, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_gamma_control_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_signal_emit_mutable(&manager->events.destroy, manager);

	assert(wl_list_empty(&manager->events.destroy.listener_list));
	assert(wl_list_empty(&manager->events.set_gamma.listener_list));

	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	free(manager);
}

struct wlr_gamma_control_manager_v1 *wlr_gamma_control_manager_v1_create(
		struct wl_display *display) {
	struct wlr_gamma_control_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (!manager) {
		return NULL;
	}

	manager->global = wl_global_create(display,
		&zwlr_gamma_control_manager_v1_interface,
		GAMMA_CONTROL_MANAGER_V1_VERSION, manager, gamma_control_manager_bind);
	if (manager->global == NULL) {
		free(manager);
		return NULL;
	}

	wl_signal_init(&manager->events.destroy);
	wl_signal_init(&manager->events.set_gamma);

	wl_list_init(&manager->controls);

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;
}

struct wlr_gamma_control_v1 *wlr_gamma_control_manager_v1_get_control(
		struct wlr_gamma_control_manager_v1 *manager, struct wlr_output *output) {
	struct wlr_gamma_control_v1 *gamma_control;
	wl_list_for_each(gamma_control, &manager->controls, link) {
		if (gamma_control->output == output) {
			return gamma_control;
		}
	}
	return NULL;
}

struct wlr_color_transform *wlr_gamma_control_v1_get_color_transform(
		struct wlr_gamma_control_v1 *gamma_control) {
	if (gamma_control == NULL || gamma_control->table == NULL) {
		return NULL;
	}

	const uint16_t *r = gamma_control->table;
	const uint16_t *g = gamma_control->table + gamma_control->ramp_size;
	const uint16_t *b = gamma_control->table + 2 * gamma_control->ramp_size;

	return wlr_color_transform_init_lut_3x1d(gamma_control->ramp_size, r, g, b);
}

bool wlr_gamma_control_v1_apply(struct wlr_gamma_control_v1 *gamma_control,
		struct wlr_output_state *output_state) {
	struct wlr_color_transform *tr = NULL;
	if (gamma_control != NULL && gamma_control->table != NULL) {
		tr = wlr_gamma_control_v1_get_color_transform(gamma_control);
		if (tr == NULL) {
			return false;
		}
	}

	wlr_output_state_set_color_transform(output_state, tr);
	return true;
}

void wlr_gamma_control_v1_send_failed_and_destroy(struct wlr_gamma_control_v1 *gamma_control) {
	if (gamma_control == NULL) {
		return;
	}
	zwlr_gamma_control_v1_send_failed(gamma_control->resource);
	gamma_control_destroy(gamma_control);
}
