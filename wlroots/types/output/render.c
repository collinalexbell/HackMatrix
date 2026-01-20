#include <assert.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/allocator.h>
#include <wlr/render/interface.h>
#include <wlr/render/swapchain.h>
#include <wlr/util/log.h>
#include <xf86drm.h>
#include "render/drm_format_set.h"
#include "render/wlr_renderer.h"
#include "render/pixel_format.h"
#include "types/wlr_output.h"

bool wlr_output_init_render(struct wlr_output *output,
		struct wlr_allocator *allocator, struct wlr_renderer *renderer) {
	assert(allocator != NULL && renderer != NULL);

	if (!(output->backend->buffer_caps & allocator->buffer_caps)) {
		wlr_log(WLR_ERROR, "output backend and allocator buffer capabilities "
			"don't match");
		return false;
	} else if (!(renderer->render_buffer_caps & allocator->buffer_caps)) {
		wlr_log(WLR_ERROR, "renderer and allocator buffer capabilities "
			"don't match");
		return false;
	}

	wlr_swapchain_destroy(output->swapchain);
	output->swapchain = NULL;

	wlr_swapchain_destroy(output->cursor_swapchain);
	output->cursor_swapchain = NULL;

	output->allocator = allocator;
	output->renderer = renderer;

	return true;
}

static struct wlr_buffer *output_acquire_empty_buffer(struct wlr_output *output,
		const struct wlr_output_state *state) {
	assert(!(state->committed & WLR_OUTPUT_STATE_BUFFER));

	// wlr_output_configure_primary_swapchain() function will call
	// wlr_output_test_state(), which can call us again. This is dangerous: we
	// risk infinite recursion. However, a buffer will always be supplied in
	// wlr_output_test_state(), which will prevent us from being called.
	if (!wlr_output_configure_primary_swapchain(output, state,
			&output->swapchain)) {
		return NULL;
	}

	struct wlr_buffer *buffer = wlr_swapchain_acquire(output->swapchain);
	if (buffer == NULL) {
		return NULL;
	}

	struct wlr_render_pass *pass =
		wlr_renderer_begin_buffer_pass(output->renderer, buffer, NULL);
	if (pass == NULL) {
		wlr_buffer_unlock(buffer);
		return NULL;
	}

	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
		.color = { 0, 0, 0, 0 },
		.blend_mode = WLR_RENDER_BLEND_MODE_NONE,
	});

	if (!wlr_render_pass_submit(pass)) {
		wlr_buffer_unlock(buffer);
		return NULL;
	}

	return buffer;
}

// This function may attach a new, empty buffer if necessary.
// If so, the new_back_buffer out parameter will be set to true.
bool output_ensure_buffer(struct wlr_output *output,
		struct wlr_output_state *state, bool *new_buffer) {
	assert(*new_buffer == false);

	// If we already have a buffer, we don't need to allocate a new one
	if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		return true;
	}

	// If the compositor hasn't called wlr_output_init_render(), they will use
	// their own logic to attach buffers
	if (output->renderer == NULL) {
		return true;
	}

	bool enabled = output->enabled;
	if (state->committed & WLR_OUTPUT_STATE_ENABLED) {
		enabled = state->enabled;
	}

	// If we're lighting up an output or changing its mode, make sure to
	// provide a new buffer
	bool needs_new_buffer = false;
	if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && state->enabled) {
		needs_new_buffer = true;
	}
	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		needs_new_buffer = true;
	}
	if (state->committed & WLR_OUTPUT_STATE_RENDER_FORMAT) {
		needs_new_buffer = true;
	}
	if (state->allow_reconfiguration && output->commit_seq == 0 && enabled) {
		// On first commit, require a new buffer if the compositor called a
		// mode-setting function, even if the mode won't change. This makes it
		// so the swapchain is created now.
		needs_new_buffer = true;
	}
	if (!needs_new_buffer) {
		return true;
	}

	wlr_log(WLR_DEBUG, "Attaching empty buffer to output for modeset");
	struct wlr_buffer *buffer = output_acquire_empty_buffer(output, state);
	if (buffer == NULL) {
		return false;
	}

	*new_buffer = true;
	wlr_output_state_set_buffer(state, buffer);
	wlr_buffer_unlock(buffer);
	return true;
}

void wlr_output_lock_attach_render(struct wlr_output *output, bool lock) {
	if (lock) {
		++output->attach_render_locks;
	} else {
		assert(output->attach_render_locks > 0);
		--output->attach_render_locks;
	}
	wlr_log(WLR_DEBUG, "%s direct scan-out on output '%s' (locks: %d)",
		lock ? "Disabling" : "Enabling", output->name,
		output->attach_render_locks);
}

bool output_pick_format(struct wlr_output *output,
		const struct wlr_drm_format_set *display_formats,
		struct wlr_drm_format *format, uint32_t fmt) {
	struct wlr_renderer *renderer = output->renderer;
	struct wlr_allocator *allocator = output->allocator;
	assert(renderer != NULL && allocator != NULL);

	const struct wlr_drm_format_set *render_formats =
		wlr_renderer_get_render_formats(renderer);
	if (render_formats == NULL) {
		wlr_log(WLR_ERROR, "Failed to get render formats");
		return false;
	}

	const struct wlr_drm_format *render_format =
		wlr_drm_format_set_get(render_formats, fmt);
	if (render_format == NULL) {
		wlr_log(WLR_DEBUG, "Renderer doesn't support format 0x%"PRIX32, fmt);
		return false;
	}

	if (display_formats != NULL) {
		const struct wlr_drm_format *display_format =
			wlr_drm_format_set_get(display_formats, fmt);
		if (display_format == NULL) {
			wlr_log(WLR_DEBUG, "Output doesn't support format 0x%"PRIX32, fmt);
			return false;
		}
		if (!wlr_drm_format_intersect(format, display_format, render_format)) {
			wlr_log(WLR_DEBUG, "Failed to intersect display and render "
				"modifiers for format 0x%"PRIX32 " on output %s",
				fmt, output->name);
			return false;
		}
	} else {
		// The output can display any format
		if (!wlr_drm_format_copy(format, render_format)) {
			return false;
		}
	}

	if (format->len == 0) {
		wlr_drm_format_finish(format);
		wlr_log(WLR_DEBUG, "Failed to pick output format");
		return false;
	}

	return true;
}

struct wlr_render_pass *wlr_output_begin_render_pass(struct wlr_output *output,
		struct wlr_output_state *state, struct wlr_buffer_pass_options *render_options) {
	if (!wlr_output_configure_primary_swapchain(output, state, &output->swapchain)) {
		return NULL;
	}

	struct wlr_buffer *buffer = wlr_swapchain_acquire(output->swapchain);
	if (buffer == NULL) {
		return NULL;
	}

	struct wlr_renderer *renderer = output->renderer;
	assert(renderer != NULL);
	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(renderer, buffer, render_options);
	if (pass == NULL) {
		return NULL;
	}

	wlr_output_state_set_buffer(state, buffer);
	wlr_buffer_unlock(buffer);
	return pass;
}
