#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-server-core.h>

#include <wlr/backend/headless.h>
#include <wlr/backend/interface.h>
#include <wlr/backend/multi.h>
#include <wlr/backend/wayland.h>
#include <wlr/config.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/log.h>
#include "types/wlr_output.h"
#include "util/env.h"
#include "util/time.h"

#if WLR_HAS_SESSION
#include <wlr/backend/session.h>
#endif

#if WLR_HAS_DRM_BACKEND
#include <wlr/backend/drm.h>
#include "backend/drm/monitor.h"
#endif

#if WLR_HAS_LIBINPUT_BACKEND
#include <wlr/backend/libinput.h>
#endif

#if WLR_HAS_X11_BACKEND
#include <wlr/backend/x11.h>
#endif

#define WAIT_SESSION_TIMEOUT 10000 // ms

void wlr_backend_init(struct wlr_backend *backend,
		const struct wlr_backend_impl *impl) {
	*backend = (struct wlr_backend){
		.impl = impl,
	};
	wl_signal_init(&backend->events.destroy);
	wl_signal_init(&backend->events.new_input);
	wl_signal_init(&backend->events.new_output);
}

void wlr_backend_finish(struct wlr_backend *backend) {
	wl_signal_emit_mutable(&backend->events.destroy, backend);

	assert(wl_list_empty(&backend->events.destroy.listener_list));
	assert(wl_list_empty(&backend->events.new_input.listener_list));
	assert(wl_list_empty(&backend->events.new_output.listener_list));
}

bool wlr_backend_start(struct wlr_backend *backend) {
	if (backend->impl->start) {
		return backend->impl->start(backend);
	}
	return true;
}

void wlr_backend_destroy(struct wlr_backend *backend) {
	if (!backend) {
		return;
	}

	if (backend->impl && backend->impl->destroy) {
		backend->impl->destroy(backend);
	} else {
		free(backend);
	}
}

static struct wlr_session *session_create_and_wait(struct wl_event_loop *loop) {
#if WLR_HAS_SESSION
	struct wlr_session *session = wlr_session_create(loop);

	if (!session) {
		wlr_log(WLR_ERROR, "Failed to start a session");
		return NULL;
	}

	if (!session->active) {
		wlr_log(WLR_INFO, "Waiting for a session to become active");

		int64_t started_at = get_current_time_msec();
		int64_t timeout = WAIT_SESSION_TIMEOUT;

		while (!session->active) {
			int ret = wl_event_loop_dispatch(loop, (int)timeout);
			if (ret < 0) {
				wlr_log_errno(WLR_ERROR, "Failed to wait for session active: "
					"wl_event_loop_dispatch failed");
				return NULL;
			}

			int64_t now = get_current_time_msec();
			if (now >= started_at + WAIT_SESSION_TIMEOUT) {
				break;
			}
			timeout = started_at + WAIT_SESSION_TIMEOUT - now;
		}

		if (!session->active) {
			wlr_log(WLR_ERROR, "Timeout waiting session to become active");
			return NULL;
		}
	}

	return session;
#else
	wlr_log(WLR_ERROR, "Cannot create session: disabled at compile-time");
	return NULL;
#endif
}

int wlr_backend_get_drm_fd(struct wlr_backend *backend) {
	if (!backend->impl->get_drm_fd) {
		return -1;
	}
	return backend->impl->get_drm_fd(backend);
}

static size_t parse_outputs_env(const char *name) {
	const char *outputs_str = getenv(name);
	if (outputs_str == NULL) {
		return 1;
	}

	char *end;
	int outputs = (int)strtol(outputs_str, &end, 10);
	if (*end || outputs < 0) {
		wlr_log(WLR_ERROR, "%s specified with invalid integer, ignoring", name);
		return 1;
	}

	return outputs;
}

/**
 * Helper to destroy the multi backend when one of its nested backends is
 * destroyed.
 */
struct wlr_auto_backend_monitor {
	struct wlr_backend *multi;
	struct wlr_backend *primary;

	struct wl_listener multi_destroy;
	struct wl_listener primary_destroy;
};

static void auto_backend_monitor_destroy(struct wlr_auto_backend_monitor *monitor) {
	wl_list_remove(&monitor->multi_destroy.link);
	wl_list_remove(&monitor->primary_destroy.link);
	free(monitor);
}

static void monitor_handle_multi_destroy(struct wl_listener *listener, void *data) {
	struct wlr_auto_backend_monitor *monitor = wl_container_of(listener, monitor, multi_destroy);
	auto_backend_monitor_destroy(monitor);
}

static void monitor_handle_primary_destroy(struct wl_listener *listener, void *data) {
	struct wlr_auto_backend_monitor *monitor = wl_container_of(listener, monitor, primary_destroy);
	wlr_backend_destroy(monitor->multi);
}

static struct wlr_auto_backend_monitor *auto_backend_monitor_create(
		struct wlr_backend *multi, struct wlr_backend *primary) {
	struct wlr_auto_backend_monitor *monitor = calloc(1, sizeof(*monitor));
	if (monitor == NULL) {
		return NULL;
	}

	monitor->multi = multi;
	monitor->primary = primary;

	monitor->multi_destroy.notify = monitor_handle_multi_destroy;
	wl_signal_add(&multi->events.destroy, &monitor->multi_destroy);

	monitor->primary_destroy.notify = monitor_handle_primary_destroy;
	wl_signal_add(&primary->events.destroy, &monitor->primary_destroy);

	return monitor;
}

static struct wlr_backend *attempt_wl_backend(struct wl_event_loop *loop) {
	struct wlr_backend *backend = wlr_wl_backend_create(loop, NULL);
	if (backend == NULL) {
		return NULL;
	}

	size_t outputs = parse_outputs_env("WLR_WL_OUTPUTS");
	for (size_t i = 0; i < outputs; ++i) {
		wlr_wl_output_create(backend);
	}

	return backend;
}

static struct wlr_backend *attempt_x11_backend(struct wl_event_loop *loop,
		const char *x11_display) {
#if WLR_HAS_X11_BACKEND
	struct wlr_backend *backend = wlr_x11_backend_create(loop, x11_display);
	if (backend == NULL) {
		return NULL;
	}

	size_t outputs = parse_outputs_env("WLR_X11_OUTPUTS");
	for (size_t i = 0; i < outputs; ++i) {
		wlr_x11_output_create(backend);
	}

	return backend;
#else
	wlr_log(WLR_ERROR, "Cannot create X11 backend: disabled at compile-time");
	return NULL;
#endif
}

static struct wlr_backend *attempt_headless_backend(struct wl_event_loop *loop) {
	struct wlr_backend *backend = wlr_headless_backend_create(loop);
	if (backend == NULL) {
		return NULL;
	}

	size_t outputs = parse_outputs_env("WLR_HEADLESS_OUTPUTS");
	for (size_t i = 0; i < outputs; ++i) {
		wlr_headless_add_output(backend, 1280, 720);
	}

	return backend;
}

static struct wlr_backend *attempt_drm_backend(struct wlr_backend *backend, struct wlr_session *session) {
#if WLR_HAS_DRM_BACKEND
	struct wlr_device *gpus[8];
	ssize_t num_gpus = wlr_session_find_gpus(session, 8, gpus);
	if (num_gpus < 0) {
		wlr_log(WLR_ERROR, "Failed to find GPUs");
		return NULL;
	}

	if (num_gpus == 0) {
		wlr_log(WLR_ERROR, "Found 0 GPUs, cannot create backend");
		return NULL;
	} else {
		wlr_log(WLR_INFO, "Found %zu GPUs", num_gpus);
	}

	struct wlr_backend *primary_drm = NULL;
	for (size_t i = 0; i < (size_t)num_gpus; ++i) {
		struct wlr_backend *drm = wlr_drm_backend_create(session, gpus[i], primary_drm);
		if (!drm) {
			wlr_log(WLR_ERROR, "Failed to create DRM backend");
			continue;
		}

		if (!primary_drm) {
			primary_drm = drm;
		}

		wlr_multi_backend_add(backend, drm);
	}
	if (!primary_drm) {
		wlr_log(WLR_ERROR, "Could not successfully create backend on any GPU");
		return NULL;
	}

	if (getenv("WLR_DRM_DEVICES") == NULL) {
		drm_backend_monitor_create(backend, primary_drm, session);
	}

	return primary_drm;
#else
	wlr_log(WLR_ERROR, "Cannot create DRM backend: disabled at compile-time");
	return NULL;
#endif
}

static struct wlr_backend *attempt_libinput_backend(struct wlr_session *session) {
#if WLR_HAS_LIBINPUT_BACKEND
	return wlr_libinput_backend_create(session);
#else
	wlr_log(WLR_ERROR, "Cannot create libinput backend: disabled at compile-time");
	return NULL;
#endif
}

static bool attempt_backend_by_name(struct wl_event_loop *loop,
		struct wlr_backend *multi, char *name,
		struct wlr_session **session_ptr) {
	struct wlr_backend *backend = NULL;
	if (strcmp(name, "wayland") == 0) {
		backend = attempt_wl_backend(loop);
	} else if (strcmp(name, "x11") == 0) {
		backend = attempt_x11_backend(loop, NULL);
	} else if (strcmp(name, "headless") == 0) {
		backend = attempt_headless_backend(loop);
	} else if (strcmp(name, "drm") == 0 || strcmp(name, "libinput") == 0) {
		// DRM and libinput need a session
		if (*session_ptr == NULL) {
			*session_ptr = session_create_and_wait(loop);
			if (*session_ptr == NULL) {
				wlr_log(WLR_ERROR, "failed to start a session");
				return false;
			}
		}

		if (strcmp(name, "libinput") == 0) {
			backend = attempt_libinput_backend(*session_ptr);
		} else {
			// attempt_drm_backend() adds the multi drm backends itself
			return attempt_drm_backend(multi, *session_ptr) != NULL;
		}
	} else {
		wlr_log(WLR_ERROR, "unrecognized backend '%s'", name);
		return false;
	}
	if (backend == NULL) {
		return false;
	}

	return wlr_multi_backend_add(multi, backend);
}

struct wlr_backend *wlr_backend_autocreate(struct wl_event_loop *loop,
		struct wlr_session **session_ptr) {
	if (session_ptr != NULL) {
		*session_ptr = NULL;
	}

	struct wlr_session *session = NULL;
	struct wlr_backend *multi = wlr_multi_backend_create(loop);
	if (!multi) {
		wlr_log(WLR_ERROR, "could not allocate multibackend");
		return NULL;
	}

	char *names = getenv("WLR_BACKENDS");
	if (names) {
		wlr_log(WLR_INFO, "Loading user-specified backends due to WLR_BACKENDS: %s",
			names);

		names = strdup(names);
		if (names == NULL) {
			wlr_log(WLR_ERROR, "allocation failed");
			goto error;
		}

		char *saveptr;
		char *name = strtok_r(names, ",", &saveptr);
		while (name != NULL) {
			if (!attempt_backend_by_name(loop, multi, name, &session)) {
				wlr_log(WLR_ERROR, "failed to add backend '%s'", name);
				free(names);
				goto error;
			}

			name = strtok_r(NULL, ",", &saveptr);
		}

		free(names);
		goto success;
	}

	if (getenv("WAYLAND_DISPLAY") || getenv("WAYLAND_SOCKET")) {
		struct wlr_backend *wl_backend = attempt_wl_backend(loop);
		if (!wl_backend) {
			goto error;
		}
		wlr_multi_backend_add(multi, wl_backend);

		if (!auto_backend_monitor_create(multi, wl_backend)) {
			goto error;
		}

		goto success;
	}

	const char *x11_display = getenv("DISPLAY");
	if (x11_display) {
		struct wlr_backend *x11_backend = attempt_x11_backend(loop, x11_display);
		if (!x11_backend) {
			goto error;
		}
		wlr_multi_backend_add(multi, x11_backend);

		if (!auto_backend_monitor_create(multi, x11_backend)) {
			goto error;
		}

		goto success;
	}

	// Attempt DRM+libinput
	session = session_create_and_wait(loop);
	if (!session) {
		wlr_log(WLR_ERROR, "Failed to start a DRM session");
		goto error;
	}

	struct wlr_backend *libinput = attempt_libinput_backend(session);
	if (libinput) {
		wlr_multi_backend_add(multi, libinput);
		if (!auto_backend_monitor_create(multi, libinput)) {
			goto error;
		}
	} else if (env_parse_bool("WLR_LIBINPUT_NO_DEVICES")) {
		wlr_log(WLR_INFO, "WLR_LIBINPUT_NO_DEVICES is set, "
			"starting without libinput backend");
	} else {
		wlr_log(WLR_ERROR, "Failed to start libinput backend");
		wlr_log(WLR_ERROR, "Set WLR_LIBINPUT_NO_DEVICES=1 to skip libinput");
		goto error;
	}

	struct wlr_backend *primary_drm = attempt_drm_backend(multi, session);
	if (primary_drm == NULL) {
		wlr_log(WLR_ERROR, "Failed to open any DRM device");
		goto error;
	}

	if (!auto_backend_monitor_create(multi, primary_drm)) {
		goto error;
	}

success:
	if (session_ptr != NULL) {
		*session_ptr = session;
	}
	return multi;

error:
	wlr_backend_destroy(multi);
#if WLR_HAS_SESSION
	wlr_session_destroy(session);
#endif
	return NULL;
}

bool wlr_backend_test(struct wlr_backend *backend,
		const struct wlr_backend_output_state *states, size_t states_len) {
	if (backend->impl->test) {
		return backend->impl->test(backend, states, states_len);
	}

	for (size_t i = 0; i < states_len; i++) {
		const struct wlr_backend_output_state *state = &states[i];
		assert(state->output->backend == backend);
		if (!wlr_output_test_state(states[i].output, &state->base)) {
			return false;
		}
	}

	return true;
}

bool wlr_backend_commit(struct wlr_backend *backend,
		const struct wlr_backend_output_state *states, size_t states_len) {
	if (!backend->impl->commit) {
		for (size_t i = 0; i < states_len; i++) {
			const struct wlr_backend_output_state *state = &states[i];
			if (!wlr_output_commit_state(state->output, &state->base)) {
				return false;
			}
		}

		return true;
	}

	for (size_t i = 0; i < states_len; i++) {
		const struct wlr_backend_output_state *state = &states[i];
		if (!output_prepare_commit(state->output, &state->base)) {
			return false;
		}
	}

	if (!backend->impl->commit(backend, states, states_len)) {
		return false;
	}

	for (size_t i = 0; i < states_len; i++) {
		const struct wlr_backend_output_state *state = &states[i];
		output_apply_commit(state->output, &state->base);
	}

	for (size_t i = 0; i < states_len; i++) {
		const struct wlr_backend_output_state *state = &states[i];
		output_send_commit_event(state->output, &state->base);
	}

	return true;
}
