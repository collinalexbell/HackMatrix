#include <assert.h>
#include <wayland-server-protocol.h>
#include <stdlib.h>

#include <wlr/types/wlr_fixes.h>

#define FIXES_VERSION 1

static void fixes_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void fixes_destroy_registry(struct wl_client *client, struct wl_resource *resource,
		struct wl_resource *registry) {
	wl_resource_destroy(registry);
}

static const struct wl_fixes_interface fixes_impl = {
	.destroy = fixes_destroy,
	.destroy_registry = fixes_destroy_registry,
};

static void fixes_bind(struct wl_client *wl_client, void *data, uint32_t version, uint32_t id) {
	struct wlr_fixes *fixes = data;

	struct wl_resource *resource = wl_resource_create(wl_client, &wl_fixes_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(wl_client);
		return;
	}
	wl_resource_set_implementation(resource, &fixes_impl, fixes, NULL);
}

static void fixes_handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_fixes *fixes = wl_container_of(listener, fixes, display_destroy);
	wl_signal_emit_mutable(&fixes->events.destroy, NULL);

	assert(wl_list_empty(&fixes->events.destroy.listener_list));

	wl_list_remove(&fixes->display_destroy.link);
	wl_global_destroy(fixes->global);
	free(fixes);
}

struct wlr_fixes *wlr_fixes_create(struct wl_display *display, uint32_t version) {
	assert(version <= FIXES_VERSION);

	struct wlr_fixes *fixes = calloc(1, sizeof(*fixes));
	if (fixes == NULL) {
		return NULL;
	}

	fixes->global = wl_global_create(display, &wl_fixes_interface, version, fixes, fixes_bind);
	if (fixes->global == NULL) {
		free(fixes);
		return NULL;
	}

	wl_signal_init(&fixes->events.destroy);

	fixes->display_destroy.notify = fixes_handle_display_destroy;
	wl_display_add_destroy_listener(display, &fixes->display_destroy);

	return fixes;
}
