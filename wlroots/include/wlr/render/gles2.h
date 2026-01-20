/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_RENDER_GLES2_H
#define WLR_RENDER_GLES2_H

#include <GLES2/gl2.h>

#include <wlr/render/wlr_renderer.h>

struct wlr_egl;

/**
 * OpenGL ES 2 renderer.
 *
 * Care must be taken to avoid stepping each other's toes with EGL contexts:
 * the current EGL is global state. The GLES2 renderer operations will save
 * and restore any previous EGL context when called. A render pass is seen as
 * a single operation.
 *
 * The GLES2 renderer doesn't support arbitrarily nested render passes. It
 * supports a subset only: after a nested render pass is created, any parent
 * render pass can't be used before the nested render pass is submitted.
 */

struct wlr_renderer *wlr_gles2_renderer_create_with_drm_fd(int drm_fd);
struct wlr_renderer *wlr_gles2_renderer_create(struct wlr_egl *egl);

struct wlr_egl *wlr_gles2_renderer_get_egl(struct wlr_renderer *renderer);
bool wlr_gles2_renderer_check_ext(struct wlr_renderer *renderer, const char *ext);
GLuint wlr_gles2_renderer_get_buffer_fbo(struct wlr_renderer *renderer, struct wlr_buffer *buffer);

struct wlr_gles2_texture_attribs {
	GLenum target; /* either GL_TEXTURE_2D or GL_TEXTURE_EXTERNAL_OES */
	GLuint tex;

	bool has_alpha;
};

bool wlr_renderer_is_gles2(struct wlr_renderer *wlr_renderer);
bool wlr_render_timer_is_gles2(struct wlr_render_timer *timer);
bool wlr_texture_is_gles2(struct wlr_texture *texture);
void wlr_gles2_texture_get_attribs(struct wlr_texture *texture,
	struct wlr_gles2_texture_attribs *attribs);

#endif
