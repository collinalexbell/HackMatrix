#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/log.h>
#include <wlr/xwayland/shell.h>
#include <wlr/xwayland/xwayland.h>
#include "xwayland/xwm.h"

struct wlr_xwayland_cursor {
	uint8_t *pixels;
	uint32_t stride;
	uint32_t width;
	uint32_t height;
	int32_t hotspot_x;
	int32_t hotspot_y;
};

static void handle_server_destroy(struct wl_listener *listener, void *data) {
	struct wlr_xwayland *xwayland =
		wl_container_of(listener, xwayland, server_destroy);
	// Server is being destroyed so avoid destroying it once again.
	xwayland->server = NULL;
	wlr_xwayland_destroy(xwayland);
}

static void handle_server_start(struct wl_listener *listener, void *data) {
	struct wlr_xwayland *xwayland =
		wl_container_of(listener, xwayland, server_start);
	if (xwayland->shell_v1 != NULL) {
		wlr_xwayland_shell_v1_set_client(xwayland->shell_v1, xwayland->server->client);
	}
}

static void xwayland_mark_ready(struct wlr_xwayland *xwayland) {
	assert(xwayland->server->wm_fd[0] >= 0);
	xwayland->xwm = xwm_create(xwayland, xwayland->server->wm_fd[0]);
	// xwm_create takes ownership of wm_fd[0] under all circumstances
	xwayland->server->wm_fd[0] = -1;

	if (!xwayland->xwm) {
		return;
	}

	if (xwayland->seat) {
		xwm_set_seat(xwayland->xwm, xwayland->seat);
	}

	if (xwayland->cursor != NULL) {
		struct wlr_xwayland_cursor *cur = xwayland->cursor;
		xwm_set_cursor(xwayland->xwm, cur->pixels, cur->stride, cur->width,
			cur->height, cur->hotspot_x, cur->hotspot_y);
	}

	wl_signal_emit_mutable(&xwayland->events.ready, NULL);
}

static void handle_server_ready(struct wl_listener *listener, void *data) {
	struct wlr_xwayland *xwayland =
		wl_container_of(listener, xwayland, server_ready);
	xwayland_mark_ready(xwayland);
}

static void handle_shell_destroy(struct wl_listener *listener, void *data) {
	struct wlr_xwayland *xwayland =
		wl_container_of(listener, xwayland, shell_destroy);
	xwayland->shell_v1 = NULL;
	wl_list_remove(&xwayland->shell_destroy.link);
	// Will remove this list in handle_shell_destroy().
	// This ensures the link is always initialized and
	// avoids the need to keep check conditions in sync.
	wl_list_init(&xwayland->shell_destroy.link);
}

void wlr_xwayland_destroy(struct wlr_xwayland *xwayland) {
	if (!xwayland) {
		return;
	}

	wl_signal_emit_mutable(&xwayland->events.destroy, NULL);

	assert(wl_list_empty(&xwayland->events.destroy.listener_list));
	assert(wl_list_empty(&xwayland->events.new_surface.listener_list));
	assert(wl_list_empty(&xwayland->events.ready.listener_list));
	assert(wl_list_empty(&xwayland->events.remove_startup_info.listener_list));

	wl_list_remove(&xwayland->server_destroy.link);
	wl_list_remove(&xwayland->server_start.link);
	wl_list_remove(&xwayland->server_ready.link);
	wl_list_remove(&xwayland->shell_destroy.link);
	free(xwayland->cursor);

	wlr_xwayland_set_seat(xwayland, NULL);
	if (xwayland->own_server) {
		wlr_xwayland_server_destroy(xwayland->server);
	}
	xwayland->server = NULL;
	wlr_xwayland_shell_v1_destroy(xwayland->shell_v1);
	xwm_destroy(xwayland->xwm);
	free(xwayland);
}

struct wlr_xwayland *wlr_xwayland_create_with_server(struct wl_display *wl_display,
		struct wlr_compositor *compositor, struct wlr_xwayland_server *server) {
	struct wlr_xwayland *xwayland = calloc(1, sizeof(*xwayland));
	if (!xwayland) {
		return NULL;
	}

	xwayland->wl_display = wl_display;
	xwayland->compositor = compositor;

	wl_signal_init(&xwayland->events.destroy);
	wl_signal_init(&xwayland->events.new_surface);
	wl_signal_init(&xwayland->events.ready);
	wl_signal_init(&xwayland->events.remove_startup_info);

	xwayland->server = server;
	xwayland->display_name = xwayland->server->display_name;

	xwayland->server_destroy.notify = handle_server_destroy;
	wl_signal_add(&xwayland->server->events.destroy, &xwayland->server_destroy);

	xwayland->server_start.notify = handle_server_start;
	wl_signal_add(&xwayland->server->events.start, &xwayland->server_start);

	xwayland->server_ready.notify = handle_server_ready;
	wl_signal_add(&xwayland->server->events.ready, &xwayland->server_ready);

	wl_list_init(&xwayland->shell_destroy.link);

	if (server->ready) {
		xwayland_mark_ready(xwayland);
	}

	return xwayland;
}

struct wlr_xwayland *wlr_xwayland_create(struct wl_display *wl_display,
		struct wlr_compositor *compositor, bool lazy) {
	struct wlr_xwayland_shell_v1 *shell_v1 = wlr_xwayland_shell_v1_create(wl_display, 1);
	if (shell_v1 == NULL) {
		return NULL;
	}

	struct wlr_xwayland_server_options options = {
		.lazy = lazy,
		.enable_wm = true,
		.terminate_delay = lazy ? 10 : 0,
	};
	struct wlr_xwayland_server *server = wlr_xwayland_server_create(wl_display, &options);
	if (server == NULL) {
		goto error_shell_v1;
	}

	struct wlr_xwayland *xwayland = wlr_xwayland_create_with_server(wl_display, compositor, server);
	if (xwayland == NULL) {
		goto error_server;
	}

	xwayland->shell_v1 = shell_v1;
	xwayland->own_server = true;

	xwayland->shell_destroy.notify = handle_shell_destroy;
	wl_signal_add(&xwayland->shell_v1->events.destroy, &xwayland->shell_destroy);

	return xwayland;

error_server:
	wlr_xwayland_server_destroy(server);
error_shell_v1:
	wlr_xwayland_shell_v1_destroy(shell_v1);
	return NULL;
}

void wlr_xwayland_set_cursor(struct wlr_xwayland *xwayland,
		uint8_t *pixels, uint32_t stride, uint32_t width, uint32_t height,
		int32_t hotspot_x, int32_t hotspot_y) {
	if (xwayland->xwm != NULL) {
		xwm_set_cursor(xwayland->xwm, pixels, stride, width, height,
			hotspot_x, hotspot_y);
		return;
	}

	free(xwayland->cursor);

	xwayland->cursor = calloc(1, sizeof(*xwayland->cursor));
	if (xwayland->cursor == NULL) {
		return;
	}
	xwayland->cursor->pixels = pixels;
	xwayland->cursor->stride = stride;
	xwayland->cursor->width = width;
	xwayland->cursor->height = height;
	xwayland->cursor->hotspot_x = hotspot_x;
	xwayland->cursor->hotspot_y = hotspot_y;
}

static void xwayland_handle_seat_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_xwayland *xwayland =
		wl_container_of(listener, xwayland, seat_destroy);

	wlr_xwayland_set_seat(xwayland, NULL);
}

void wlr_xwayland_set_seat(struct wlr_xwayland *xwayland,
		struct wlr_seat *seat) {
	if (xwayland->seat) {
		wl_list_remove(&xwayland->seat_destroy.link);
	}

	xwayland->seat = seat;

	if (xwayland->xwm) {
		xwm_set_seat(xwayland->xwm, seat);
	}

	if (seat == NULL) {
		return;
	}

	xwayland->seat_destroy.notify = xwayland_handle_seat_destroy;
	wl_signal_add(&seat->events.destroy, &xwayland->seat_destroy);
}
