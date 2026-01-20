#include <assert.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_output_swapchain_manager.h>
#include <wlr/util/log.h>
#include "render/drm_format_set.h"
#include "types/wlr_output.h"

struct wlr_output_swapchain_manager_output {
	struct wlr_output *output;
	// Newly allocated swapchain. Can be NULL if the old swapchain is re-used
	// or if the output is disabled.
	struct wlr_swapchain *new_swapchain;
	// True if the output was included in the last successful call to
	// wlr_output_swapchain_manager_prepare().
	bool test_success;
	// Pending swapchain which will replace the old one when
	// wlr_output_swapchain_manager_apply() is called. Can be either a pointer
	// to the newly allocated swapchain, or the old swapchain, or NULL.
	struct wlr_swapchain *pending_swapchain;
};

void wlr_output_swapchain_manager_init(struct wlr_output_swapchain_manager *manager,
		struct wlr_backend *backend) {
	*manager = (struct wlr_output_swapchain_manager){
		.backend = backend,
	};
}

static struct wlr_output_swapchain_manager_output *manager_get_output(
		struct wlr_output_swapchain_manager *manager, struct wlr_output *output) {
	struct wlr_output_swapchain_manager_output *manager_output;
	wl_array_for_each(manager_output, &manager->outputs) {
		if (manager_output->output == output) {
			return manager_output;
		}
	}
	return NULL;
}

static struct wlr_output_swapchain_manager_output *manager_get_or_add_output(
		struct wlr_output_swapchain_manager *manager, struct wlr_output *output) {
	struct wlr_output_swapchain_manager_output *manager_output = manager_get_output(manager, output);
	if (manager_output != NULL) {
		return manager_output;
	}

	manager_output = wl_array_add(&manager->outputs, sizeof(*manager_output));
	if (manager_output == NULL) {
		return NULL;
	}
	*manager_output = (struct wlr_output_swapchain_manager_output){
		.output = output,
	};
	return manager_output;
}

static bool swapchain_is_compatible(struct wlr_swapchain *swapchain,
		int width, int height, const struct wlr_drm_format *format) {
	if (swapchain == NULL) {
		return false;
	}
	if (swapchain->width != width || swapchain->height != height) {
		return false;
	}
	if (swapchain->format.format != format->format || swapchain->format.len != format->len) {
		return false;
	}
	assert(format->len > 0);
	return memcmp(swapchain->format.modifiers, format->modifiers, format->len * sizeof(format->modifiers[0])) == 0;
}

static struct wlr_swapchain *manager_output_get_swapchain(
		struct wlr_output_swapchain_manager_output *manager_output,
		int width, int height, const struct wlr_drm_format *format) {
	struct wlr_output *output = manager_output->output;

	if (swapchain_is_compatible(output->swapchain, width, height, format)) {
		return output->swapchain;
	}
	if (swapchain_is_compatible(manager_output->new_swapchain, width, height, format)) {
		return manager_output->new_swapchain;
	}

	struct wlr_swapchain *swapchain = wlr_swapchain_create(output->allocator, width, height, format);
	if (swapchain == NULL) {
		return NULL;
	}
	wlr_swapchain_destroy(manager_output->new_swapchain);
	manager_output->new_swapchain = swapchain;
	return swapchain;
}

static bool manager_output_prepare(struct wlr_output_swapchain_manager_output *manager_output,
		struct wlr_output_state *state, bool explicit_modifiers) {
	struct wlr_output *output = manager_output->output;
	struct wlr_allocator *allocator = output->allocator;
	assert(allocator != NULL);

	if (!output_pending_enabled(output, state)) {
		manager_output->pending_swapchain = NULL;
		return true;
	}

	int width, height;
	output_pending_resolution(output, state, &width, &height);

	uint32_t fmt = output->render_format;
	if (state->committed & WLR_OUTPUT_STATE_RENDER_FORMAT) {
		fmt = state->render_format;
	}

	const struct wlr_drm_format_set *display_formats =
		wlr_output_get_primary_formats(output, allocator->buffer_caps);
	struct wlr_drm_format format = {0};
	if (!output_pick_format(output, display_formats, &format, fmt)) {
		return false;
	}

	if (!explicit_modifiers && (format.len != 1 || format.modifiers[0] != DRM_FORMAT_MOD_LINEAR)) {
		if (!wlr_drm_format_has(&format, DRM_FORMAT_MOD_INVALID)) {
			wlr_log(WLR_DEBUG, "Implicit modifiers not supported");
			wlr_drm_format_finish(&format);
			return false;
		}

		format.len = 0;
		if (!wlr_drm_format_add(&format, DRM_FORMAT_MOD_INVALID)) {
			wlr_log(WLR_DEBUG, "Failed to add implicit modifier to format");
			wlr_drm_format_finish(&format);
			return false;
		}
	}

	struct wlr_swapchain *swapchain =
		manager_output_get_swapchain(manager_output, width, height, &format);
	wlr_drm_format_finish(&format);
	if (swapchain == NULL) {
		return false;
	}

	struct wlr_buffer *buffer = wlr_swapchain_acquire(swapchain);
	if (buffer == NULL) {
		return false;
	}

	wlr_output_state_set_buffer(state, buffer);
	wlr_buffer_unlock(buffer);
	manager_output->pending_swapchain = swapchain;
	return true;
}

static bool manager_test(struct wlr_output_swapchain_manager *manager,
		struct wlr_backend_output_state *states, size_t states_len,
		bool explicit_modifiers) {
	wlr_log(WLR_DEBUG, "Preparing test commit for %zu outputs with %s modifiers",
		states_len, explicit_modifiers ? "explicit": "implicit");

	struct wlr_output_swapchain_manager_output *manager_output;
	wl_array_for_each(manager_output, &manager->outputs) {
		manager_output->test_success = false;
	}

	for (size_t i = 0; i < states_len; i++) {
		struct wlr_backend_output_state *state = &states[i];
		struct wlr_output_swapchain_manager_output *manager_output =
			manager_get_or_add_output(manager, state->output);
		if (manager_output == NULL) {
			return false;
		}
		if (!manager_output_prepare(manager_output, &state->base, explicit_modifiers)) {
			return false;
		}
	}

	bool ok = wlr_backend_test(manager->backend, states, states_len);
	wlr_log(WLR_DEBUG, "Test commit for %zu outputs %s",
		states_len, ok ? "succeeded" : "failed");
	if (!ok) {
		return false;
	}

	for (size_t i = 0; i < states_len; i++) {
		struct wlr_output_swapchain_manager_output *manager_output =
			manager_get_output(manager, states[i].output);
		assert(manager_output != NULL);
		manager_output->test_success = true;
	}

	return true;
}

bool wlr_output_swapchain_manager_prepare(struct wlr_output_swapchain_manager *manager,
		const struct wlr_backend_output_state *states, size_t states_len) {
	bool ok = false;
	struct wlr_backend_output_state *pending = malloc(states_len * sizeof(states[0]));
	if (pending == NULL) {
		return false;
	}
	for (size_t i = 0; i < states_len; i++) {
		pending[i] = states[i];
		pending[i].base.buffer = NULL;
	}

	ok = manager_test(manager, pending, states_len, true);
	if (!ok) {
		ok = manager_test(manager, pending, states_len, false);
	}

	for (size_t i = 0; i < states_len; i++) {
		wlr_buffer_unlock(pending[i].base.buffer);
	}
	free(pending);

	return ok;
}

struct wlr_swapchain *wlr_output_swapchain_manager_get_swapchain(
		struct wlr_output_swapchain_manager *manager, struct wlr_output *output) {
	struct wlr_output_swapchain_manager_output *manager_output =
		manager_get_output(manager, output);
	assert(manager_output != NULL && manager_output->test_success);
	return manager_output->pending_swapchain;
}

void wlr_output_swapchain_manager_apply(struct wlr_output_swapchain_manager *manager) {
	struct wlr_output_swapchain_manager_output *manager_output;
	wl_array_for_each(manager_output, &manager->outputs) {
		struct wlr_output *output = manager_output->output;
		if (!manager_output->test_success || manager_output->pending_swapchain == output->swapchain) {
			continue;
		}

		wlr_swapchain_destroy(output->swapchain);
		output->swapchain = manager_output->new_swapchain;
		manager_output->new_swapchain = NULL;
		manager_output->test_success = false;
	}
}

void wlr_output_swapchain_manager_finish(struct wlr_output_swapchain_manager *manager) {
	struct wlr_output_swapchain_manager_output *manager_output;
	wl_array_for_each(manager_output, &manager->outputs) {
		wlr_swapchain_destroy(manager_output->new_swapchain);
	}
	wl_array_release(&manager->outputs);
}
