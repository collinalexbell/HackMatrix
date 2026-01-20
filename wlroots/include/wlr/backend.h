/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_BACKEND_H
#define WLR_BACKEND_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>

struct wlr_session;
struct wlr_backend_impl;

/**
 * Per-output state for wlr_backend_test() and wlr_backend_commit().
 */
struct wlr_backend_output_state {
	struct wlr_output *output;
	struct wlr_output_state base;
};

/**
 * A backend provides a set of input and output devices.
 *
 * Buffer capabilities and features can change over the lifetime of a backend,
 * for instance when a child backend is added to a multi-backend.
 */
struct wlr_backend {
	const struct wlr_backend_impl *impl;

	// Bitfield of supported buffer capabilities (see enum wlr_buffer_cap)
	uint32_t buffer_caps;

	struct {
		// Whether wait/signal timelines are supported in output commits
		bool timeline;
	} features;

	struct {
		/** Raised when destroyed */
		struct wl_signal destroy;
		/** Raised when new inputs are added, passed the struct wlr_input_device */
		struct wl_signal new_input;
		/** Raised when new outputs are added, passed the struct wlr_output */
		struct wl_signal new_output;
	} events;
};

/**
 * Automatically initializes the most suitable backend given the environment.
 * Will always return a multi-backend. The backend is created but not started.
 * Returns NULL on failure.
 *
 * If session_ptr is not NULL, it's populated with the session which has been
 * created with the backend, if any.
 *
 * The multi-backend will be destroyed if one of the primary underlying
 * backends is destroyed (e.g. if the primary DRM device is unplugged).
 */
struct wlr_backend *wlr_backend_autocreate(struct wl_event_loop *loop,
	struct wlr_session **session_ptr);
/**
 * Start the backend. This may signal new_input or new_output immediately, but
 * may also wait until the event loop is started. Returns false on failure.
 */
bool wlr_backend_start(struct wlr_backend *backend);
/**
 * Destroy the backend and clean up all of its resources. Normally called
 * automatically when the event loop is destroyed.
 */
void wlr_backend_destroy(struct wlr_backend *backend);
/**
 * Returns the DRM node file descriptor used by the backend's underlying
 * platform. Can be used by consumers for additional rendering operations.
 * The consumer must not close the file descriptor since the backend continues
 * to have ownership of it.
 */
int wlr_backend_get_drm_fd(struct wlr_backend *backend);

/**
 * Atomically test a new configuration for multiple outputs.
 *
 * Some backends (e.g. DRM) have global backend-wide limitations. This function
 * can be used to check whether changes across multiple outputs are supported by
 * the backend.
 */
bool wlr_backend_test(struct wlr_backend *backend,
	const struct wlr_backend_output_state *states, size_t states_len);
/**
 * Atomically apply a new configuration for multiple outputs.
 *
 * There is no guarantee that the changes will be applied atomically. Users
 * should call wlr_backend_test() first to check that the new state is supported
 * by the backend.
 */
bool wlr_backend_commit(struct wlr_backend *backend,
	const struct wlr_backend_output_state *states, size_t states_len);

#endif
