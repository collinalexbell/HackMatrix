#ifndef TYPES_WLR_OUTPUT_H
#define TYPES_WLR_OUTPUT_H

#include <wlr/render/drm_format_set.h>
#include <wlr/types/wlr_output.h>

void output_pending_resolution(struct wlr_output *output,
	const struct wlr_output_state *state, int *width, int *height);
bool output_pending_enabled(struct wlr_output *output,
	const struct wlr_output_state *state);
const struct wlr_output_image_description *output_pending_image_description(
	struct wlr_output *output, const struct wlr_output_state *state);

bool output_pick_format(struct wlr_output *output,
	const struct wlr_drm_format_set *display_formats,
	struct wlr_drm_format *format, uint32_t fmt);
bool output_ensure_buffer(struct wlr_output *output,
	struct wlr_output_state *state, bool *new_back_buffer);

bool output_cursor_set_texture(struct wlr_output_cursor *cursor,
	struct wlr_texture *texture, bool own_texture, const struct wlr_fbox *src_box,
	int dst_width, int dst_height, enum wl_output_transform transform,
	int32_t hotspot_x, int32_t hotspot_y, struct wlr_drm_syncobj_timeline *wait_timeline,
	uint64_t wait_point);
bool output_cursor_refresh_color_transform(struct wlr_output_cursor *cursor,
	const struct wlr_output_image_description *img_desc);

void output_defer_present(struct wlr_output *output, struct wlr_output_event_present event);

bool output_prepare_commit(struct wlr_output *output, const struct wlr_output_state *state);
void output_apply_commit(struct wlr_output *output, const struct wlr_output_state *state);
void output_send_commit_event(struct wlr_output *output, const struct wlr_output_state *state);

void output_state_get_buffer_src_box(const struct wlr_output_state *state,
	struct wlr_fbox *out);
void output_state_get_buffer_dst_box(const struct wlr_output_state *state,
	struct wlr_box *out);

#endif
