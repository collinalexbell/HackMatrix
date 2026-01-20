#include <stdlib.h>
#include <string.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/render/color.h>
#include <wlr/util/log.h>
#include "types/wlr_output.h"

void wlr_output_state_init(struct wlr_output_state *state) {
	*state = (struct wlr_output_state){0};
	pixman_region32_init(&state->damage);
}

void wlr_output_state_finish(struct wlr_output_state *state) {
	wlr_buffer_unlock(state->buffer);
	// struct wlr_buffer is ref'counted, so the pointer may remain valid after
	// wlr_buffer_unlock(). Reset the field to NULL to ensure nobody mistakenly
	// reads it after output_state_finish().
	state->buffer = NULL;
	pixman_region32_fini(&state->damage);
	wlr_drm_syncobj_timeline_unref(state->wait_timeline);
	wlr_drm_syncobj_timeline_unref(state->signal_timeline);
	wlr_color_transform_unref(state->color_transform);
	free(state->image_description);
}

void wlr_output_state_set_enabled(struct wlr_output_state *state,
		bool enabled) {
	state->committed |= WLR_OUTPUT_STATE_ENABLED;
	state->enabled = enabled;
	state->allow_reconfiguration = true;
}

void wlr_output_state_set_mode(struct wlr_output_state *state,
		struct wlr_output_mode *mode) {
	state->committed |= WLR_OUTPUT_STATE_MODE;
	state->mode_type = WLR_OUTPUT_STATE_MODE_FIXED;
	state->mode = mode;
	state->allow_reconfiguration = true;
}

void wlr_output_state_set_custom_mode(struct wlr_output_state *state,
		int32_t width, int32_t height, int32_t refresh) {
	state->committed |= WLR_OUTPUT_STATE_MODE;
	state->mode_type = WLR_OUTPUT_STATE_MODE_CUSTOM;
	state->custom_mode.width = width;
	state->custom_mode.height = height;
	state->custom_mode.refresh = refresh;
	state->allow_reconfiguration = true;
}

void wlr_output_state_set_scale(struct wlr_output_state *state, float scale) {
	state->committed |= WLR_OUTPUT_STATE_SCALE;
	state->scale = scale;
}

void wlr_output_state_set_transform(struct wlr_output_state *state,
		enum wl_output_transform transform) {
	state->committed |= WLR_OUTPUT_STATE_TRANSFORM;
	state->transform = transform;
}

void wlr_output_state_set_adaptive_sync_enabled(struct wlr_output_state *state,
		bool enabled) {
	state->committed |= WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED;
	state->adaptive_sync_enabled = enabled;
}

void wlr_output_state_set_render_format(struct wlr_output_state *state,
		uint32_t format) {
	state->committed |= WLR_OUTPUT_STATE_RENDER_FORMAT;
	state->render_format = format;
}

void wlr_output_state_set_subpixel(struct wlr_output_state *state,
		enum wl_output_subpixel subpixel) {
	state->committed |= WLR_OUTPUT_STATE_SUBPIXEL;
	state->subpixel = subpixel;
}

void wlr_output_state_set_buffer(struct wlr_output_state *state,
		struct wlr_buffer *buffer) {
	state->committed |= WLR_OUTPUT_STATE_BUFFER;
	wlr_buffer_unlock(state->buffer);
	state->buffer = wlr_buffer_lock(buffer);
}

void wlr_output_state_set_damage(struct wlr_output_state *state,
		const pixman_region32_t *damage) {
	state->committed |= WLR_OUTPUT_STATE_DAMAGE;
	pixman_region32_copy(&state->damage, damage);
}

void wlr_output_state_set_layers(struct wlr_output_state *state,
		struct wlr_output_layer_state *layers, size_t layers_len) {
	state->committed |= WLR_OUTPUT_STATE_LAYERS;
	state->layers = layers;
	state->layers_len = layers_len;
}

void wlr_output_state_set_wait_timeline(struct wlr_output_state *state,
		struct wlr_drm_syncobj_timeline *timeline, uint64_t src_point) {
	state->committed |= WLR_OUTPUT_STATE_WAIT_TIMELINE;
	wlr_drm_syncobj_timeline_unref(state->wait_timeline);
	state->wait_timeline = wlr_drm_syncobj_timeline_ref(timeline);
	state->wait_point = src_point;
}

void wlr_output_state_set_signal_timeline(struct wlr_output_state *state,
		struct wlr_drm_syncobj_timeline *timeline, uint64_t dst_point) {
	state->committed |= WLR_OUTPUT_STATE_SIGNAL_TIMELINE;
	wlr_drm_syncobj_timeline_unref(state->signal_timeline);
	state->signal_timeline = wlr_drm_syncobj_timeline_ref(timeline);
	state->signal_point = dst_point;
}

void wlr_output_state_set_color_transform(struct wlr_output_state *state,
		struct wlr_color_transform *tr) {
	state->committed |= WLR_OUTPUT_STATE_COLOR_TRANSFORM;
	wlr_color_transform_unref(state->color_transform);
	if (tr) {
		state->color_transform = wlr_color_transform_ref(tr);
	} else {
		state->color_transform = NULL;
	}
}

bool wlr_output_state_set_image_description(struct wlr_output_state *state,
		const struct wlr_output_image_description *image_desc) {
	struct wlr_output_image_description *copy = NULL;
	if (image_desc != NULL) {
		copy = malloc(sizeof(*copy));
		if (copy == NULL) {
			return false;
		}
		*copy = *image_desc;
	}

	state->committed |= WLR_OUTPUT_STATE_IMAGE_DESCRIPTION;
	free(state->image_description);
	state->image_description = copy;
	return true;
}

bool wlr_output_state_copy(struct wlr_output_state *dst,
		const struct wlr_output_state *src) {
	struct wlr_output_state copy = *src;
	copy.committed &= ~(WLR_OUTPUT_STATE_BUFFER |
		WLR_OUTPUT_STATE_DAMAGE |
		WLR_OUTPUT_STATE_WAIT_TIMELINE |
		WLR_OUTPUT_STATE_SIGNAL_TIMELINE |
		WLR_OUTPUT_STATE_COLOR_TRANSFORM |
		WLR_OUTPUT_STATE_IMAGE_DESCRIPTION);
	copy.buffer = NULL;
	copy.buffer_src_box = (struct wlr_fbox){0};
	copy.buffer_dst_box = (struct wlr_box){0};
	pixman_region32_init(&copy.damage);
	copy.wait_timeline = NULL;
	copy.signal_timeline = NULL;
	copy.color_transform = NULL;
	copy.image_description = NULL;

	if (src->committed & WLR_OUTPUT_STATE_BUFFER) {
		wlr_output_state_set_buffer(&copy, src->buffer);
		copy.buffer_src_box = src->buffer_src_box;
		copy.buffer_dst_box = src->buffer_dst_box;
	}

	if (src->committed & WLR_OUTPUT_STATE_DAMAGE) {
		wlr_output_state_set_damage(&copy, &src->damage);
	}

	if (src->committed & WLR_OUTPUT_STATE_WAIT_TIMELINE) {
		wlr_output_state_set_wait_timeline(&copy, src->wait_timeline,
			src->wait_point);
	}
	if (src->committed & WLR_OUTPUT_STATE_SIGNAL_TIMELINE) {
		wlr_output_state_set_signal_timeline(&copy, src->signal_timeline,
			src->signal_point);
	}

	if (src->committed & WLR_OUTPUT_STATE_COLOR_TRANSFORM) {
		wlr_output_state_set_color_transform(&copy, src->color_transform);
	}
	if (src->committed & WLR_OUTPUT_STATE_IMAGE_DESCRIPTION) {
		if (!wlr_output_state_set_image_description(&copy, src->image_description)) {
			goto err;
		}
	}

	wlr_output_state_finish(dst);
	*dst = copy;
	return true;

err:
	wlr_output_state_finish(dst);
	return false;
}
