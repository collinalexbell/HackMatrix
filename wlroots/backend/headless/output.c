#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/types/wlr_output_layer.h>
#include <wlr/util/log.h>
#include "backend/headless.h"
#include "types/wlr_output.h"

static const uint32_t SUPPORTED_OUTPUT_STATE =
	WLR_OUTPUT_STATE_BACKEND_OPTIONAL |
	WLR_OUTPUT_STATE_BUFFER |
	WLR_OUTPUT_STATE_ENABLED |
	WLR_OUTPUT_STATE_MODE;

static size_t last_output_num = 0;

static struct wlr_headless_output *headless_output_from_output(
		struct wlr_output *wlr_output) {
	assert(wlr_output_is_headless(wlr_output));
	struct wlr_headless_output *output = wl_container_of(wlr_output, output, wlr_output);
	return output;
}

static void output_update_refresh(struct wlr_headless_output *output,
		int32_t refresh) {
	if (refresh <= 0) {
		refresh = HEADLESS_DEFAULT_REFRESH;
	}

	output->frame_delay = 1000000 / refresh;
}

static bool output_test(struct wlr_output *wlr_output,
		const struct wlr_output_state *state) {
	uint32_t unsupported = state->committed & ~SUPPORTED_OUTPUT_STATE;
	if (unsupported != 0) {
		wlr_log(WLR_DEBUG, "Unsupported output state fields: 0x%"PRIx32,
			unsupported);
		return false;
	}

	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		assert(state->mode_type == WLR_OUTPUT_STATE_MODE_CUSTOM);
	}

	if (state->committed & WLR_OUTPUT_STATE_LAYERS) {
		for (size_t i = 0; i < state->layers_len; i++) {
			state->layers[i].accepted = true;
		}
	}

	return true;
}

static bool output_commit(struct wlr_output *wlr_output,
		const struct wlr_output_state *state) {
	struct wlr_headless_output *output =
		headless_output_from_output(wlr_output);

	if (!output_test(wlr_output, state)) {
		return false;
	}

	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		output_update_refresh(output, state->custom_mode.refresh);
	}

	if (output_pending_enabled(wlr_output, state)) {
		struct wlr_output_event_present present_event = {
			.commit_seq = wlr_output->commit_seq + 1,
			.presented = true,
		};
		output_defer_present(wlr_output, present_event);

		wl_event_source_timer_update(output->frame_timer, output->frame_delay);
	}

	return true;
}

static bool output_set_cursor(struct wlr_output *wlr_output,
		struct wlr_buffer *buffer, int hotspot_x, int hotspot_y) {
	return true;
}

static bool output_move_cursor(struct wlr_output *wlr_output, int x, int y) {
	return true;
}

static void output_destroy(struct wlr_output *wlr_output) {
	struct wlr_headless_output *output = headless_output_from_output(wlr_output);

	wlr_output_finish(wlr_output);

	wl_list_remove(&output->link);
	wl_event_source_remove(output->frame_timer);
	free(output);
}

static const struct wlr_output_impl output_impl = {
	.destroy = output_destroy,
	.test = output_test,
	.commit = output_commit,
	.set_cursor = output_set_cursor,
	.move_cursor = output_move_cursor,
};

bool wlr_output_is_headless(struct wlr_output *wlr_output) {
	return wlr_output->impl == &output_impl;
}

static int signal_frame(void *data) {
	struct wlr_headless_output *output = data;
	wlr_output_send_frame(&output->wlr_output);
	return 0;
}

struct wlr_output *wlr_headless_add_output(struct wlr_backend *wlr_backend,
		unsigned int width, unsigned int height) {
	struct wlr_headless_backend *backend =
		headless_backend_from_backend(wlr_backend);

	struct wlr_headless_output *output = calloc(1, sizeof(*output));
	if (output == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate wlr_headless_output");
		return NULL;
	}
	output->backend = backend;
	struct wlr_output *wlr_output = &output->wlr_output;

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_custom_mode(&state, width, height, 0);

	wlr_output_init(wlr_output, &backend->backend, &output_impl, backend->event_loop, &state);
	wlr_output_state_finish(&state);

	output_update_refresh(output, 0);

	size_t output_num = ++last_output_num;

	char name[64];
	snprintf(name, sizeof(name), "HEADLESS-%zu", output_num);
	wlr_output_set_name(wlr_output, name);

	char description[128];
	snprintf(description, sizeof(description), "Headless output %zu", output_num);
	wlr_output_set_description(wlr_output, description);

	output->frame_timer = wl_event_loop_add_timer(backend->event_loop, signal_frame, output);

	wl_list_insert(&backend->outputs, &output->link);

	if (backend->started) {
		wl_signal_emit_mutable(&backend->backend.events.new_output, wlr_output);
	}

	return wlr_output;
}
