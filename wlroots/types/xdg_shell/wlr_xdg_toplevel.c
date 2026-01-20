#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include <wlr/util/edges.h>
#include "types/wlr_xdg_shell.h"
#include "util/utf8.h"

void handle_xdg_toplevel_ack_configure(
		struct wlr_xdg_toplevel *toplevel,
		struct wlr_xdg_toplevel_configure *configure) {
	toplevel->pending.maximized = configure->maximized;
	toplevel->pending.fullscreen = configure->fullscreen;
	toplevel->pending.resizing = configure->resizing;
	toplevel->pending.activated = configure->activated;
	toplevel->pending.tiled = configure->tiled;
	toplevel->pending.constrained = configure->constrained;
	toplevel->pending.suspended = configure->suspended;

	toplevel->pending.width = configure->width;
	toplevel->pending.height = configure->height;
}

struct wlr_xdg_toplevel_configure *send_xdg_toplevel_configure(
		struct wlr_xdg_toplevel *toplevel) {
	struct wlr_xdg_toplevel_configure *configure =
		calloc(1, sizeof(*configure));
	if (configure == NULL) {
		wlr_log(WLR_ERROR, "Allocation failed");
		wl_resource_post_no_memory(toplevel->resource);
		return NULL;
	}
	*configure = toplevel->scheduled;

	uint32_t version = wl_resource_get_version(toplevel->resource);

	if ((configure->fields & WLR_XDG_TOPLEVEL_CONFIGURE_BOUNDS) &&
			version >= XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION) {
		xdg_toplevel_send_configure_bounds(toplevel->resource,
			configure->bounds.width, configure->bounds.height);
	}

	if ((configure->fields & WLR_XDG_TOPLEVEL_CONFIGURE_WM_CAPABILITIES) &&
			version >= XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION) {
		size_t caps_len = 0;
		uint32_t caps[32];
		if (configure->wm_capabilities & WLR_XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU) {
			caps[caps_len++] = XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU;
		}
		if (configure->wm_capabilities & WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE) {
			caps[caps_len++] = XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE;
		}
		if (configure->wm_capabilities & WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN) {
			caps[caps_len++] = XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN;
		}
		if (configure->wm_capabilities & WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE) {
			caps[caps_len++] = XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;
		}
		assert(caps_len <= sizeof(caps) / sizeof(caps[0]));

		struct wl_array caps_array = {
			.size = caps_len * sizeof(caps[0]),
			.data = caps,
		};
		xdg_toplevel_send_wm_capabilities(toplevel->resource, &caps_array);
	}

	size_t nstates = 0;
	uint32_t states[32];
	if (configure->maximized) {
		states[nstates++] = XDG_TOPLEVEL_STATE_MAXIMIZED;
	}
	if (configure->fullscreen) {
		states[nstates++] = XDG_TOPLEVEL_STATE_FULLSCREEN;
	}
	if (configure->resizing) {
		states[nstates++] = XDG_TOPLEVEL_STATE_RESIZING;
	}
	if (configure->activated) {
		states[nstates++] = XDG_TOPLEVEL_STATE_ACTIVATED;
	}
	if (configure->tiled && version >= XDG_TOPLEVEL_STATE_TILED_LEFT_SINCE_VERSION) {
		static const struct {
			enum wlr_edges edge;
			enum xdg_toplevel_state state;
		} tiled[] = {
			{ WLR_EDGE_LEFT, XDG_TOPLEVEL_STATE_TILED_LEFT },
			{ WLR_EDGE_RIGHT, XDG_TOPLEVEL_STATE_TILED_RIGHT },
			{ WLR_EDGE_TOP, XDG_TOPLEVEL_STATE_TILED_TOP },
			{ WLR_EDGE_BOTTOM, XDG_TOPLEVEL_STATE_TILED_BOTTOM },
		};

		for (size_t i = 0; i < sizeof(tiled) / sizeof(tiled[0]); ++i) {
			if ((configure->tiled & tiled[i].edge) == 0) {
				continue;
			}
			states[nstates++] = tiled[i].state;
		}
	}
	if (configure->suspended && version >= XDG_TOPLEVEL_STATE_SUSPENDED_SINCE_VERSION) {
		states[nstates++] = XDG_TOPLEVEL_STATE_SUSPENDED;
	}
	if (configure->constrained && version >= XDG_TOPLEVEL_STATE_CONSTRAINED_LEFT_SINCE_VERSION) {
		static const struct {
			enum wlr_edges edge;
			enum xdg_toplevel_state state;
		} constrained[] = {
			{ WLR_EDGE_LEFT, XDG_TOPLEVEL_STATE_CONSTRAINED_LEFT },
			{ WLR_EDGE_RIGHT, XDG_TOPLEVEL_STATE_CONSTRAINED_RIGHT },
			{ WLR_EDGE_TOP, XDG_TOPLEVEL_STATE_CONSTRAINED_TOP },
			{ WLR_EDGE_BOTTOM, XDG_TOPLEVEL_STATE_CONSTRAINED_BOTTOM },
		};

		for (size_t i = 0; i < sizeof(constrained) / sizeof(constrained[0]); ++i) {
			if ((configure->constrained & constrained[i].edge) == 0) {
				continue;
			}
			states[nstates++] = constrained[i].state;
		}
	}
	assert(nstates <= sizeof(states) / sizeof(states[0]));

	int32_t width = configure->width;
	int32_t height = configure->height;
	struct wl_array wl_states = {
		.size = nstates * sizeof(states[0]),
		.data = states,
	};
	xdg_toplevel_send_configure(toplevel->resource,
		width, height, &wl_states);

	toplevel->scheduled.fields = 0;

	return configure;
}

void handle_xdg_toplevel_client_commit(struct wlr_xdg_toplevel *toplevel) {
	struct wlr_xdg_toplevel_state *pending = &toplevel->pending;

	// 1) Negative values are prohibited
	// 2) If both min and max are set (aren't 0), min ≤ max
	if (pending->min_width < 0 || pending->min_height < 0 ||
			pending->max_width < 0 || pending->max_height < 0 ||
			(pending->max_width != 0 && pending->max_width < pending->min_width) ||
			(pending->max_height != 0 && pending->max_height < pending->min_height)) {
		wlr_surface_reject_pending(toplevel->base->surface, toplevel->resource,
			XDG_TOPLEVEL_ERROR_INVALID_SIZE, "client provided an invalid min or max size");
		return;
	}
}

static const struct xdg_toplevel_interface xdg_toplevel_implementation;

struct wlr_xdg_toplevel *wlr_xdg_toplevel_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &xdg_toplevel_interface,
		&xdg_toplevel_implementation));
	return wl_resource_get_user_data(resource);
}

struct wlr_xdg_toplevel *wlr_xdg_toplevel_try_from_wlr_surface(struct wlr_surface *surface) {
	struct wlr_xdg_surface *xdg_surface = wlr_xdg_surface_try_from_wlr_surface(surface);
	if (xdg_surface == NULL || xdg_surface->role != WLR_XDG_SURFACE_ROLE_TOPLEVEL) {
		return NULL;
	}
	return xdg_surface->toplevel;
}

static void handle_parent_unmap(struct wl_listener *listener, void *data) {
	struct wlr_xdg_toplevel *toplevel =
		wl_container_of(listener, toplevel, parent_unmap);
	if (!wlr_xdg_toplevel_set_parent(toplevel, toplevel->parent->parent)) {
		assert(0 && "Unreachable");
	}
}

bool wlr_xdg_toplevel_set_parent(struct wlr_xdg_toplevel *toplevel,
		struct wlr_xdg_toplevel *parent) {
	// Check for a loop
	struct wlr_xdg_toplevel *iter = parent;
	while (iter != NULL) {
		if (iter == toplevel) {
			return false;
		}
		iter = iter->parent;
	}

	if (toplevel->parent != NULL) {
		wl_list_remove(&toplevel->parent_unmap.link);
	}

	if (parent != NULL && parent->base->surface->mapped) {
		toplevel->parent = parent;
		toplevel->parent_unmap.notify = handle_parent_unmap;
		wl_signal_add(&toplevel->parent->base->surface->events.unmap,
			&toplevel->parent_unmap);
	} else {
		toplevel->parent = NULL;
	}

	wl_signal_emit_mutable(&toplevel->events.set_parent, NULL);
	return true;
}

static void xdg_toplevel_handle_set_parent(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *parent_resource) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	struct wlr_xdg_toplevel *parent = NULL;

	if (parent_resource != NULL) {
		parent = wlr_xdg_toplevel_from_resource(parent_resource);
	}

	if (!wlr_xdg_toplevel_set_parent(toplevel, parent)) {
		wl_resource_post_error(resource, XDG_TOPLEVEL_ERROR_INVALID_PARENT,
			"a toplevel cannot be a parent of itself or its ancestor");
	}
}

static void xdg_toplevel_handle_set_title(struct wl_client *client,
		struct wl_resource *resource, const char *title) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	char *tmp;

	if (!is_utf8(title)) {
		// TODO: update when xdg_toplevel has a dedicated error code for this
		wl_resource_post_error(resource, (uint32_t)-1, "xdg_toplevel title is not valid UTF-8");
		return;
	}

	tmp = strdup(title);
	if (tmp == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	free(toplevel->title);
	toplevel->title = tmp;
	wl_signal_emit_mutable(&toplevel->events.set_title, NULL);
}

static void xdg_toplevel_handle_set_app_id(struct wl_client *client,
		struct wl_resource *resource, const char *app_id) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	char *tmp;

	tmp = strdup(app_id);
	if (tmp == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	free(toplevel->app_id);
	toplevel->app_id = tmp;
	wl_signal_emit_mutable(&toplevel->events.set_app_id, NULL);
}

static void xdg_toplevel_handle_show_window_menu(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *seat_resource,
		uint32_t serial, int32_t x, int32_t y) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	struct wlr_seat_client *seat =
		wlr_seat_client_from_resource(seat_resource);

	if (!toplevel->base->configured) {
		wl_resource_post_error(toplevel->base->resource,
			XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
			"surface has not been configured yet");
		return;
	}

	struct wlr_xdg_toplevel_show_window_menu_event event = {
		.toplevel = toplevel,
		.seat = seat,
		.serial = serial,
		.x = x,
		.y = y,
	};

	wl_signal_emit_mutable(&toplevel->events.request_show_window_menu, &event);
}

static void xdg_toplevel_handle_move(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *seat_resource,
		uint32_t serial) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	struct wlr_seat_client *seat =
		wlr_seat_client_from_resource(seat_resource);

	if (!toplevel->base->configured) {
		wl_resource_post_error(toplevel->base->resource,
			XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
			"surface has not been configured yet");
		return;
	}

	struct wlr_xdg_toplevel_move_event event = {
		.toplevel = toplevel,
		.seat = seat,
		.serial = serial,
	};

	wl_signal_emit_mutable(&toplevel->events.request_move, &event);
}

static void xdg_toplevel_handle_resize(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *seat_resource,
		uint32_t serial, uint32_t edges) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	struct wlr_seat_client *seat =
		wlr_seat_client_from_resource(seat_resource);

	uint32_t version = wl_resource_get_version(toplevel->base->resource);
	if (!xdg_toplevel_resize_edge_is_valid(edges, version)) {
		wl_resource_post_error(toplevel->base->resource,
			XDG_TOPLEVEL_ERROR_INVALID_RESIZE_EDGE,
			"provided value is not a valid variant of the resize_edge enum");
		return;
	}

	if (!toplevel->base->configured) {
		wl_resource_post_error(toplevel->base->resource,
			XDG_SURFACE_ERROR_NOT_CONSTRUCTED,
			"surface has not been configured yet");
		return;
	}

	struct wlr_xdg_toplevel_resize_event event = {
		.toplevel = toplevel,
		.seat = seat,
		.serial = serial,
		.edges = edges,
	};

	wl_signal_emit_mutable(&toplevel->events.request_resize, &event);
}

static void xdg_toplevel_handle_set_max_size(struct wl_client *client,
		struct wl_resource *resource, int32_t width, int32_t height) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	toplevel->pending.max_width = width;
	toplevel->pending.max_height = height;
}

static void xdg_toplevel_handle_set_min_size(struct wl_client *client,
		struct wl_resource *resource, int32_t width, int32_t height) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	toplevel->pending.min_width = width;
	toplevel->pending.min_height = height;
}

static void xdg_toplevel_handle_set_maximized(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	toplevel->requested.maximized = true;
	wl_signal_emit_mutable(&toplevel->events.request_maximize, NULL);
}

static void xdg_toplevel_handle_unset_maximized(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	toplevel->requested.maximized = false;
	wl_signal_emit_mutable(&toplevel->events.request_maximize, NULL);
}

static void handle_fullscreen_output_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_xdg_toplevel_requested *req =
		wl_container_of(listener, req, fullscreen_output_destroy);
	req->fullscreen_output = NULL;
	wl_list_remove(&req->fullscreen_output_destroy.link);
}

static void store_fullscreen_requested(struct wlr_xdg_toplevel *toplevel,
		bool fullscreen, struct wlr_output *output) {
	struct wlr_xdg_toplevel_requested *req = &toplevel->requested;
	req->fullscreen = fullscreen;
	if (req->fullscreen_output) {
		wl_list_remove(&req->fullscreen_output_destroy.link);
	}
	req->fullscreen_output = output;
	if (req->fullscreen_output) {
		req->fullscreen_output_destroy.notify =
			handle_fullscreen_output_destroy;
		wl_signal_add(&req->fullscreen_output->events.destroy,
				&req->fullscreen_output_destroy);
	}
}

static void xdg_toplevel_handle_set_fullscreen(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *output_resource) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);

	struct wlr_output *output = NULL;
	if (output_resource != NULL) {
		output = wlr_output_from_resource(output_resource);
	}

	store_fullscreen_requested(toplevel, true, output);

	wl_signal_emit_mutable(&toplevel->events.request_fullscreen, NULL);
}

static void xdg_toplevel_handle_unset_fullscreen(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);

	store_fullscreen_requested(toplevel, false, NULL);

	wl_signal_emit_mutable(&toplevel->events.request_fullscreen, NULL);
}

static void xdg_toplevel_handle_set_minimized(struct wl_client *client,
		struct wl_resource *resource) {
	struct wlr_xdg_toplevel *toplevel =
		wlr_xdg_toplevel_from_resource(resource);
	toplevel->requested.minimized = true;
	wl_signal_emit_mutable(&toplevel->events.request_minimize, NULL);
}

static void xdg_toplevel_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct xdg_toplevel_interface xdg_toplevel_implementation = {
	.destroy = xdg_toplevel_handle_destroy,
	.set_parent = xdg_toplevel_handle_set_parent,
	.set_title = xdg_toplevel_handle_set_title,
	.set_app_id = xdg_toplevel_handle_set_app_id,
	.show_window_menu = xdg_toplevel_handle_show_window_menu,
	.move = xdg_toplevel_handle_move,
	.resize = xdg_toplevel_handle_resize,
	.set_max_size = xdg_toplevel_handle_set_max_size,
	.set_min_size = xdg_toplevel_handle_set_min_size,
	.set_maximized = xdg_toplevel_handle_set_maximized,
	.unset_maximized = xdg_toplevel_handle_unset_maximized,
	.set_fullscreen = xdg_toplevel_handle_set_fullscreen,
	.unset_fullscreen = xdg_toplevel_handle_unset_fullscreen,
	.set_minimized = xdg_toplevel_handle_set_minimized,
};

static const struct wlr_surface_synced_impl surface_synced_impl = {
	.state_size = sizeof(struct wlr_xdg_toplevel_state),
};

void create_xdg_toplevel(struct wlr_xdg_surface *surface,
		uint32_t id) {
	if (!set_xdg_surface_role(surface, WLR_XDG_SURFACE_ROLE_TOPLEVEL)) {
		return;
	}

	assert(surface->toplevel == NULL);
	surface->toplevel = calloc(1, sizeof(*surface->toplevel));
	if (surface->toplevel == NULL) {
		wl_resource_post_no_memory(surface->resource);
		return;
	}
	surface->toplevel->base = surface;

	wl_signal_init(&surface->toplevel->events.destroy);
	wl_signal_init(&surface->toplevel->events.request_maximize);
	wl_signal_init(&surface->toplevel->events.request_fullscreen);
	wl_signal_init(&surface->toplevel->events.request_minimize);
	wl_signal_init(&surface->toplevel->events.request_move);
	wl_signal_init(&surface->toplevel->events.request_resize);
	wl_signal_init(&surface->toplevel->events.request_show_window_menu);
	wl_signal_init(&surface->toplevel->events.set_parent);
	wl_signal_init(&surface->toplevel->events.set_title);
	wl_signal_init(&surface->toplevel->events.set_app_id);

	if (!wlr_surface_synced_init(&surface->toplevel->synced, surface->surface,
			&surface_synced_impl, &surface->toplevel->pending, &surface->toplevel->current)) {
		goto error_toplevel;
	}

	surface->toplevel->resource = wl_resource_create(
		surface->client->client, &xdg_toplevel_interface,
		wl_resource_get_version(surface->resource), id);
	if (surface->toplevel->resource == NULL) {
		goto error_synced;
	}
	wl_resource_set_implementation(surface->toplevel->resource,
		&xdg_toplevel_implementation, surface->toplevel, NULL);

	set_xdg_surface_role_object(surface, surface->toplevel->resource);

	if (surface->client->shell->version >= XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION) {
		// The first configure event must carry WM capabilities
		surface->toplevel->scheduled.wm_capabilities =
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU |
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE |
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN |
			WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE;
		surface->toplevel->scheduled.fields |= WLR_XDG_TOPLEVEL_CONFIGURE_WM_CAPABILITIES;
	}

	wl_signal_emit_mutable(&surface->client->shell->events.new_toplevel, surface->toplevel);

	return;

error_synced:
	wlr_surface_synced_finish(&surface->toplevel->synced);
error_toplevel:
	free(surface->toplevel);
	surface->toplevel = NULL;
	wl_resource_post_no_memory(surface->resource);
}

void reset_xdg_toplevel(struct wlr_xdg_toplevel *toplevel) {
	if (toplevel->parent) {
		wl_list_remove(&toplevel->parent_unmap.link);
		toplevel->parent = NULL;
	}
	free(toplevel->title);
	toplevel->title = NULL;
	free(toplevel->app_id);
	toplevel->app_id = NULL;

	if (toplevel->requested.fullscreen_output) {
		wl_list_remove(&toplevel->requested.fullscreen_output_destroy.link);
		toplevel->requested.fullscreen_output = NULL;
	}
	toplevel->requested.fullscreen = false;
	toplevel->requested.maximized = false;
	toplevel->requested.minimized = false;
}

void destroy_xdg_toplevel(struct wlr_xdg_toplevel *toplevel) {
	wlr_surface_unmap(toplevel->base->surface);
	reset_xdg_toplevel(toplevel);

	wl_signal_emit_mutable(&toplevel->events.destroy, NULL);

	assert(wl_list_empty(&toplevel->events.destroy.listener_list));
	assert(wl_list_empty(&toplevel->events.request_maximize.listener_list));
	assert(wl_list_empty(&toplevel->events.request_fullscreen.listener_list));
	assert(wl_list_empty(&toplevel->events.request_minimize.listener_list));
	assert(wl_list_empty(&toplevel->events.request_move.listener_list));
	assert(wl_list_empty(&toplevel->events.request_resize.listener_list));
	assert(wl_list_empty(&toplevel->events.request_show_window_menu.listener_list));
	assert(wl_list_empty(&toplevel->events.set_parent.listener_list));
	assert(wl_list_empty(&toplevel->events.set_title.listener_list));
	assert(wl_list_empty(&toplevel->events.set_app_id.listener_list));

	wlr_surface_synced_finish(&toplevel->synced);
	toplevel->base->toplevel = NULL;
	wl_resource_set_user_data(toplevel->resource, NULL);
	free(toplevel);
}

void wlr_xdg_toplevel_send_close(struct wlr_xdg_toplevel *toplevel) {
	xdg_toplevel_send_close(toplevel->resource);
}

uint32_t wlr_xdg_toplevel_configure(struct wlr_xdg_toplevel *toplevel,
		const struct wlr_xdg_toplevel_configure *configure) {
	toplevel->scheduled.width = configure->width;
	toplevel->scheduled.height = configure->height;
	toplevel->scheduled.maximized = configure->maximized;
	toplevel->scheduled.fullscreen = configure->fullscreen;
	toplevel->scheduled.resizing = configure->resizing;
	toplevel->scheduled.activated = configure->activated;
	toplevel->scheduled.suspended = configure->suspended;
	toplevel->scheduled.tiled = configure->tiled;

	if (configure->fields & WLR_XDG_TOPLEVEL_CONFIGURE_BOUNDS) {
		toplevel->scheduled.fields |= WLR_XDG_TOPLEVEL_CONFIGURE_BOUNDS;
		toplevel->scheduled.bounds = configure->bounds;
	}

	if (configure->fields & WLR_XDG_TOPLEVEL_CONFIGURE_WM_CAPABILITIES) {
		toplevel->scheduled.fields |= WLR_XDG_TOPLEVEL_CONFIGURE_WM_CAPABILITIES;
		toplevel->scheduled.wm_capabilities = configure->wm_capabilities;
	}

	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel *toplevel,
		int32_t width, int32_t height) {
	assert(width >= 0 && height >= 0);
	toplevel->scheduled.width = width;
	toplevel->scheduled.height = height;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_activated(struct wlr_xdg_toplevel *toplevel,
		bool activated) {
	toplevel->scheduled.activated = activated;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_maximized(struct wlr_xdg_toplevel *toplevel,
		bool maximized) {
	toplevel->scheduled.maximized = maximized;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_fullscreen(struct wlr_xdg_toplevel *toplevel,
		bool fullscreen) {
	toplevel->scheduled.fullscreen = fullscreen;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_resizing(struct wlr_xdg_toplevel *toplevel,
		bool resizing) {
	toplevel->scheduled.resizing = resizing;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_tiled(struct wlr_xdg_toplevel *toplevel,
		uint32_t tiled) {
	assert(toplevel->base->client->shell->version >=
		XDG_TOPLEVEL_STATE_TILED_LEFT_SINCE_VERSION);
	toplevel->scheduled.tiled = tiled;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_bounds(struct wlr_xdg_toplevel *toplevel,
		int32_t width, int32_t height) {
	assert(toplevel->base->client->shell->version >=
		XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION);
	assert(width >= 0 && height >= 0);
	toplevel->scheduled.fields |= WLR_XDG_TOPLEVEL_CONFIGURE_BOUNDS;
	toplevel->scheduled.bounds.width = width;
	toplevel->scheduled.bounds.height = height;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_wm_capabilities(struct wlr_xdg_toplevel *toplevel,
		uint32_t caps) {
	assert(toplevel->base->client->shell->version >=
		XDG_TOPLEVEL_WM_CAPABILITIES_SINCE_VERSION);
	toplevel->scheduled.fields |= WLR_XDG_TOPLEVEL_CONFIGURE_WM_CAPABILITIES;
	toplevel->scheduled.wm_capabilities = caps;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_suspended(struct wlr_xdg_toplevel *toplevel,
		bool suspended) {
	assert(toplevel->base->client->shell->version >=
		XDG_TOPLEVEL_STATE_SUSPENDED_SINCE_VERSION);
	toplevel->scheduled.suspended = suspended;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}

uint32_t wlr_xdg_toplevel_set_constrained(struct wlr_xdg_toplevel *toplevel, uint32_t constrained) {
	assert(toplevel->base->client->shell->version >=
		XDG_TOPLEVEL_STATE_CONSTRAINED_LEFT_SINCE_VERSION);
	toplevel->scheduled.constrained = constrained;
	return wlr_xdg_surface_schedule_configure(toplevel->base);
}
