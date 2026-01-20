/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_BACKEND_INTERFACE_H
#define WLR_BACKEND_INTERFACE_H

#include <stdbool.h>
#include <wlr/backend.h>

struct wlr_output_state;

struct wlr_backend_impl {
	bool (*start)(struct wlr_backend *backend);
	void (*destroy)(struct wlr_backend *backend);
	int (*get_drm_fd)(struct wlr_backend *backend);
	bool (*test)(struct wlr_backend *backend,
		const struct wlr_backend_output_state *states, size_t states_len);
	bool (*commit)(struct wlr_backend *backend,
		const struct wlr_backend_output_state *states, size_t states_len);
};

/**
 * Initializes common state on a struct wlr_backend and sets the implementation
 * to the provided struct wlr_backend_impl reference.
 */
void wlr_backend_init(struct wlr_backend *backend,
		const struct wlr_backend_impl *impl);
/**
 * Emit the destroy event and clean up common backend state.
 */
void wlr_backend_finish(struct wlr_backend *backend);

#endif
