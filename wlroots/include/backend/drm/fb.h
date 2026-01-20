#ifndef BACKEND_DRM_FB_H
#define BACKEND_DRM_FB_H

#include <stdbool.h>
#include <stdint.h>
#include <wlr/util/addon.h>

struct wlr_drm_format_set;

struct wlr_drm_fb {
	struct wlr_buffer *wlr_buf;
	struct wlr_addon addon;
	struct wlr_drm_backend *backend;
	struct wl_list link; // wlr_drm_backend.fbs

	uint32_t id;
};

bool drm_fb_import(struct wlr_drm_fb **fb, struct wlr_drm_backend *drm,
		struct wlr_buffer *buf, const struct wlr_drm_format_set *formats);
void drm_fb_destroy(struct wlr_drm_fb *fb);

void drm_fb_clear(struct wlr_drm_fb **fb);
void drm_fb_copy(struct wlr_drm_fb **new, struct wlr_drm_fb *old);
void drm_fb_move(struct wlr_drm_fb **new, struct wlr_drm_fb **old);
struct wlr_drm_fb *drm_fb_lock(struct wlr_drm_fb *fb);

#endif
