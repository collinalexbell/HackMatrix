#ifndef RENDER_WLR_RENDERER_H
#define RENDER_WLR_RENDERER_H

#include <wlr/render/wlr_renderer.h>

/**
 * Automatically select and create a renderer suitable for the DRM FD.
 */
struct wlr_renderer *renderer_autocreate_with_drm_fd(int drm_fd);
/**
 * Get the supported render formats. Buffers allocated with a format from this
 * list may be used with wlr_renderer_begin_buffer_pass().
 */
const struct wlr_drm_format_set *wlr_renderer_get_render_formats(
	struct wlr_renderer *renderer);

#endif
