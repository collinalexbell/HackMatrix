#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <wlr/backend/interface.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/util/log.h>
#include "backend/multi.h"

struct subbackend_state {
	struct wlr_backend *backend;
	struct wlr_backend *container;
	struct wl_listener new_input;
	struct wl_listener new_output;
	struct wl_listener destroy;
	struct wl_list link;
};

static struct wlr_multi_backend *multi_backend_from_backend(
		struct wlr_backend *wlr_backend) {
	assert(wlr_backend_is_multi(wlr_backend));
	struct wlr_multi_backend *backend = wl_container_of(wlr_backend, backend, backend);
	return backend;
}

static bool multi_backend_start(struct wlr_backend *wlr_backend) {
	struct wlr_multi_backend *backend = multi_backend_from_backend(wlr_backend);
	struct subbackend_state *sub;
	wl_list_for_each(sub, &backend->backends, link) {
		if (!wlr_backend_start(sub->backend)) {
			wlr_log(WLR_ERROR, "Failed to initialize backend.");
			return false;
		}
	}
	return true;
}

static void subbackend_state_destroy(struct subbackend_state *sub) {
	wl_list_remove(&sub->new_input.link);
	wl_list_remove(&sub->new_output.link);
	wl_list_remove(&sub->destroy.link);
	wl_list_remove(&sub->link);
	free(sub);
}

static void multi_backend_destroy(struct wlr_backend *wlr_backend) {
	struct wlr_multi_backend *backend = multi_backend_from_backend(wlr_backend);

	wl_list_remove(&backend->event_loop_destroy.link);

	wlr_backend_finish(wlr_backend);

	assert(wl_list_empty(&backend->events.backend_add.listener_list));
	assert(wl_list_empty(&backend->events.backend_remove.listener_list));

	// Some backends may depend on other backends, ie. destroying a backend may
	// also destroy other backends
	while (!wl_list_empty(&backend->backends)) {
		struct subbackend_state *sub =
			wl_container_of(backend->backends.next, sub, link);
		wlr_backend_destroy(sub->backend);
	}

	free(backend);
}

static int multi_backend_get_drm_fd(struct wlr_backend *backend) {
	struct wlr_multi_backend *multi = multi_backend_from_backend(backend);

	struct subbackend_state *sub;
	wl_list_for_each(sub, &multi->backends, link) {
		if (sub->backend->impl->get_drm_fd) {
			return wlr_backend_get_drm_fd(sub->backend);
		}
	}

	return -1;
}

static int compare_output_state_backend(const void *data_a, const void *data_b) {
	const struct wlr_backend_output_state *a = data_a;
	const struct wlr_backend_output_state *b = data_b;

	uintptr_t ptr_a = (uintptr_t)a->output->backend;
	uintptr_t ptr_b = (uintptr_t)b->output->backend;

	if (ptr_a == ptr_b) {
		return 0;
	} else if (ptr_a < ptr_b) {
		return -1;
	} else {
		return 1;
	}
}

static bool commit(struct wlr_backend *backend,
		const struct wlr_backend_output_state *states, size_t states_len,
		bool test_only) {
	// Group states by backend, then perform one commit per backend
	struct wlr_backend_output_state *by_backend = malloc(states_len * sizeof(by_backend[0]));
	if (by_backend == NULL) {
		return false;
	}
	memcpy(by_backend, states, states_len * sizeof(by_backend[0]));
	qsort(by_backend, states_len, sizeof(by_backend[0]), compare_output_state_backend);

	bool ok = true;
	for (size_t i = 0; i < states_len;) {
		struct wlr_backend *sub = by_backend[i].output->backend;

		size_t len = 1;
		while (i + len < states_len &&
				by_backend[i + len].output->backend == sub) {
			len++;
		}

		if (test_only) {
			ok = wlr_backend_test(sub, &by_backend[i], len);
		} else {
			ok = wlr_backend_commit(sub, &by_backend[i], len);
		}
		if (!ok) {
			break;
		}
		i += len;
	}

	free(by_backend);
	return ok;
}

static bool multi_backend_test(struct wlr_backend *backend,
		const struct wlr_backend_output_state *states, size_t states_len) {
	return commit(backend, states, states_len, true);
}

static bool multi_backend_commit(struct wlr_backend *backend,
		const struct wlr_backend_output_state *states, size_t states_len) {
	return commit(backend, states, states_len, false);
}

static const struct wlr_backend_impl backend_impl = {
	.start = multi_backend_start,
	.destroy = multi_backend_destroy,
	.get_drm_fd = multi_backend_get_drm_fd,
	.test = multi_backend_test,
	.commit = multi_backend_commit,
};

static void handle_event_loop_destroy(struct wl_listener *listener, void *data) {
	struct wlr_multi_backend *backend =
		wl_container_of(listener, backend, event_loop_destroy);
	multi_backend_destroy((struct wlr_backend*)backend);
}

struct wlr_backend *wlr_multi_backend_create(struct wl_event_loop *loop) {
	struct wlr_multi_backend *backend = calloc(1, sizeof(*backend));
	if (!backend) {
		wlr_log(WLR_ERROR, "Backend allocation failed");
		return NULL;
	}

	wl_list_init(&backend->backends);
	wlr_backend_init(&backend->backend, &backend_impl);

	wl_signal_init(&backend->events.backend_add);
	wl_signal_init(&backend->events.backend_remove);

	backend->event_loop_destroy.notify = handle_event_loop_destroy;
	wl_event_loop_add_destroy_listener(loop, &backend->event_loop_destroy);

	return &backend->backend;
}

bool wlr_backend_is_multi(struct wlr_backend *b) {
	return b->impl == &backend_impl;
}

static void new_input_reemit(struct wl_listener *listener, void *data) {
	struct subbackend_state *state = wl_container_of(listener,
			state, new_input);
	wl_signal_emit_mutable(&state->container->events.new_input, data);
}

static void new_output_reemit(struct wl_listener *listener, void *data) {
	struct subbackend_state *state = wl_container_of(listener,
			state, new_output);
	wl_signal_emit_mutable(&state->container->events.new_output, data);
}

static void handle_subbackend_destroy(struct wl_listener *listener,
		void *data) {
	struct subbackend_state *state = wl_container_of(listener, state, destroy);
	subbackend_state_destroy(state);
}

static struct subbackend_state *multi_backend_get_subbackend(struct wlr_multi_backend *multi,
		struct wlr_backend *backend) {
	struct subbackend_state *sub = NULL;
	wl_list_for_each(sub, &multi->backends, link) {
		if (sub->backend == backend) {
			return sub;
		}
	}
	return NULL;
}

static void multi_backend_refresh_features(struct wlr_multi_backend *multi) {
	multi->backend.buffer_caps = 0;
	multi->backend.features.timeline = true;

	bool has_buffer_cap = false;
	uint32_t buffer_caps_intersection =
		WLR_BUFFER_CAP_DATA_PTR | WLR_BUFFER_CAP_DMABUF | WLR_BUFFER_CAP_SHM;
	struct subbackend_state *sub = NULL;
	wl_list_for_each(sub, &multi->backends, link) {
		// Only take into account backends capable of presenting a buffer
		if (sub->backend->buffer_caps != 0) {
			has_buffer_cap = true;
			buffer_caps_intersection &= sub->backend->buffer_caps;
		}

		// timeline is only applicable to backends that support DMABUFs
		if (sub->backend->buffer_caps & WLR_BUFFER_CAP_DMABUF) {
			multi->backend.features.timeline = multi->backend.features.timeline &&
				sub->backend->features.timeline;
		}
	}

	if (has_buffer_cap) {
		multi->backend.buffer_caps = buffer_caps_intersection;
	}
}

bool wlr_multi_backend_add(struct wlr_backend *_multi,
		struct wlr_backend *backend) {
	assert(_multi && backend);
	assert(_multi != backend);

	struct wlr_multi_backend *multi = multi_backend_from_backend(_multi);

	if (multi_backend_get_subbackend(multi, backend)) {
		// already added
		return true;
	}

	struct subbackend_state *sub = calloc(1, sizeof(*sub));
	if (sub == NULL) {
		wlr_log(WLR_ERROR, "Could not add backend: allocation failed");
		return false;
	}
	wl_list_insert(multi->backends.prev, &sub->link);

	sub->backend = backend;
	sub->container = &multi->backend;

	wl_signal_add(&backend->events.destroy, &sub->destroy);
	sub->destroy.notify = handle_subbackend_destroy;

	wl_signal_add(&backend->events.new_input, &sub->new_input);
	sub->new_input.notify = new_input_reemit;

	wl_signal_add(&backend->events.new_output, &sub->new_output);
	sub->new_output.notify = new_output_reemit;

	multi_backend_refresh_features(multi);
	wl_signal_emit_mutable(&multi->events.backend_add, backend);
	return true;
}

void wlr_multi_backend_remove(struct wlr_backend *_multi,
		struct wlr_backend *backend) {
	struct wlr_multi_backend *multi = multi_backend_from_backend(_multi);

	struct subbackend_state *sub =
		multi_backend_get_subbackend(multi, backend);

	if (sub) {
		wl_signal_emit_mutable(&multi->events.backend_remove, backend);
		subbackend_state_destroy(sub);
		multi_backend_refresh_features(multi);
	}
}

bool wlr_multi_is_empty(struct wlr_backend *_backend) {
	assert(wlr_backend_is_multi(_backend));
	struct wlr_multi_backend *backend = (struct wlr_multi_backend *)_backend;
	return wl_list_length(&backend->backends) < 1;
}

void wlr_multi_for_each_backend(struct wlr_backend *_backend,
		void (*callback)(struct wlr_backend *backend, void *data), void *data) {
	assert(wlr_backend_is_multi(_backend));
	struct wlr_multi_backend *backend = (struct wlr_multi_backend *)_backend;
	struct subbackend_state *sub;
	wl_list_for_each(sub, &backend->backends, link) {
		callback(sub->backend, data);
	}
}
