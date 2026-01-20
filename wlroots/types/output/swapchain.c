#include <assert.h>
#include <drm_fourcc.h>
#include <stdlib.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/util/log.h>
#include <xf86drm.h>

#include "render/drm_format_set.h"
#include "types/wlr_output.h"

static struct wlr_swapchain *create_swapchain(struct wlr_output *output,
		int width, int height, uint32_t render_format, bool allow_modifiers) {
	struct wlr_allocator *allocator = output->allocator;
	assert(output->allocator != NULL);

	const struct wlr_drm_format_set *display_formats =
		wlr_output_get_primary_formats(output, allocator->buffer_caps);
	struct wlr_drm_format format = {0};
	if (!output_pick_format(output, display_formats, &format, render_format)) {
		wlr_log(WLR_ERROR, "Failed to pick primary buffer format for output '%s'",
			output->name);
		return NULL;
	}

	char *format_name = drmGetFormatName(format.format);
	wlr_log(WLR_DEBUG, "Choosing primary buffer format %s (0x%08"PRIX32") for output '%s'",
		format_name ? format_name : "<unknown>", format.format, output->name);
	free(format_name);

	if (!allow_modifiers && (format.len != 1 || format.modifiers[0] != DRM_FORMAT_MOD_LINEAR)) {
		if (!wlr_drm_format_has(&format, DRM_FORMAT_MOD_INVALID)) {
			wlr_log(WLR_DEBUG, "Implicit modifiers not supported");
			wlr_drm_format_finish(&format);
			return NULL;
		}

		format.len = 0;
		if (!wlr_drm_format_add(&format, DRM_FORMAT_MOD_INVALID)) {
			wlr_log(WLR_DEBUG, "Failed to add implicit modifier to format");
			wlr_drm_format_finish(&format);
			return NULL;
		}
	}

	struct wlr_swapchain *swapchain = wlr_swapchain_create(allocator, width, height, &format);
	wlr_drm_format_finish(&format);
	return swapchain;
}

static bool test_swapchain(struct wlr_output *output,
		struct wlr_swapchain *swapchain, const struct wlr_output_state *state) {
	struct wlr_buffer *buffer = wlr_swapchain_acquire(swapchain);
	if (buffer == NULL) {
		return false;
	}

	struct wlr_output_state copy = *state;
	copy.committed |= WLR_OUTPUT_STATE_BUFFER;
	copy.buffer = buffer;
	bool ok = wlr_output_test_state(output, &copy);
	wlr_buffer_unlock(buffer);
	return ok;
}

bool wlr_output_configure_primary_swapchain(struct wlr_output *output,
		const struct wlr_output_state *state, struct wlr_swapchain **swapchain_ptr) {
	struct wlr_output_state empty_state;
	if (state == NULL) {
		wlr_output_state_init(&empty_state);
		state = &empty_state;
	}

	int width, height;
	output_pending_resolution(output, state, &width, &height);

	uint32_t format = output->render_format;
	if (state->committed & WLR_OUTPUT_STATE_RENDER_FORMAT) {
		format = state->render_format;
	}

	// Re-use the existing swapchain if possible
	struct wlr_swapchain *old_swapchain = *swapchain_ptr;
	if (old_swapchain != NULL &&
			old_swapchain->width == width && old_swapchain->height == height &&
			old_swapchain->format.format == format) {
		return true;
	}

	struct wlr_swapchain *swapchain = create_swapchain(output, width, height, format, true);
	if (swapchain == NULL) {
		wlr_log(WLR_ERROR, "Failed to create swapchain for output '%s'", output->name);
		return false;
	}

	wlr_log(WLR_DEBUG, "Testing swapchain for output '%s'", output->name);
	if (!test_swapchain(output, swapchain, state)) {
		wlr_log(WLR_DEBUG, "Output test failed on '%s', retrying without modifiers",
			output->name);
		wlr_swapchain_destroy(swapchain);
		swapchain = create_swapchain(output, width, height, format, false);
		if (swapchain == NULL) {
			wlr_log(WLR_ERROR, "Failed to create modifier-less swapchain for output '%s'",
				output->name);
			return false;
		}
		wlr_log(WLR_DEBUG, "Testing modifier-less swapchain for output '%s'", output->name);
		if (!test_swapchain(output, swapchain, state)) {
			wlr_log(WLR_ERROR, "Swapchain for output '%s' failed test", output->name);
			wlr_swapchain_destroy(swapchain);
			return false;
		}
	}

	wlr_swapchain_destroy(*swapchain_ptr);
	*swapchain_ptr = swapchain;
	return true;
}
