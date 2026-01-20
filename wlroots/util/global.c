#include <stdlib.h>
#include "util/global.h"

struct destroy_global_data {
	struct wl_global *global;
	struct wl_event_source *event_source;
	struct wl_listener display_destroy;
};

static void destroy_global(struct destroy_global_data *data) {
	wl_list_remove(&data->display_destroy.link);
	wl_global_destroy(data->global);
	wl_event_source_remove(data->event_source);
	free(data);
}

static int handle_timer_event(void *data) {
	destroy_global(data);
	return 0;
}

static void handle_display_destroy(struct wl_listener *listener, void *_data) {
	struct destroy_global_data *data =
		wl_container_of(listener, data, display_destroy);
	destroy_global(data);
}

void wlr_global_destroy_safe(struct wl_global *global) {
	// Don't destroy the global immediately. If the global has been created
	// recently, clients might try to bind to it after we've destroyed it.
	// Instead, remove the global so that clients stop seeing it and wait an
	// arbitrary amount of time before destroying the global as a workaround.
	// See: https://gitlab.freedesktop.org/wayland/wayland/issues/10

	wl_global_remove(global);
	wl_global_set_user_data(global, NULL); // safety net

	struct wl_display *display = wl_global_get_display(global);
	struct wl_event_loop *event_loop = wl_display_get_event_loop(display);
	struct destroy_global_data *data = calloc(1, sizeof(*data));
	if (data == NULL) {
		wl_global_destroy(global);
		return;
	}
	data->global = global;
	data->event_source =
		wl_event_loop_add_timer(event_loop, handle_timer_event, data);
	if (data->event_source == NULL) {
		free(data);
		wl_global_destroy(global);
		return;
	}
	wl_event_source_timer_update(data->event_source, 5000);

	data->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &data->display_destroy);
}
