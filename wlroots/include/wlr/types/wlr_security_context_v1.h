/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_SECURITY_CONTEXT_V1_H
#define WLR_TYPES_WLR_SECURITY_CONTEXT_V1_H

#include <wayland-server-core.h>

/**
 * An implementation of the security context protocol.
 *
 * Compositors can create this manager, setup a filter for Wayland globals via
 * wl_display_set_global_filter(), and inside the filter query the security
 * context state via wlr_security_context_manager_v1_lookup_client().
 */
struct wlr_security_context_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
		struct wl_signal commit; // struct wlr_security_context_v1_commit_event
	} events;

	void *data;

	struct {
		struct wl_list contexts; // wlr_security_context_v1.link

		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_security_context_v1_state {
	char *sandbox_engine; // may be NULL
	char *app_id; // may be NULL
	char *instance_id; // may be NULL
};

struct wlr_security_context_v1_commit_event {
	const struct wlr_security_context_v1_state *state;
	// Client which created the security context
	struct wl_client *parent_client;
};

struct wlr_security_context_manager_v1 *wlr_security_context_manager_v1_create(
	struct wl_display *display);
const struct wlr_security_context_v1_state *wlr_security_context_manager_v1_lookup_client(
	struct wlr_security_context_manager_v1 *manager, const struct wl_client *client);

#endif
