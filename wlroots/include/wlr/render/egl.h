/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_RENDER_EGL_H
#define WLR_RENDER_EGL_H

#ifndef EGL_NO_X11
#define EGL_NO_X11
#endif
#ifndef EGL_NO_PLATFORM_SPECIFIC_TYPES
#define EGL_NO_PLATFORM_SPECIFIC_TYPES
#endif

#include <wlr/config.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <pixman.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/render/dmabuf.h>
#include <wlr/render/drm_format_set.h>

struct wlr_egl;

/**
 * Create a struct wlr_egl with an existing EGL display and context.
 *
 * This is typically used by compositors which want to customize EGL
 * initialization.
 */
struct wlr_egl *wlr_egl_create_with_context(EGLDisplay display,
	EGLContext context);

/**
 * Get the EGL display used by the struct wlr_egl.
 *
 * This is typically used by compositors which need to perform custom OpenGL
 * operations.
 */
EGLDisplay wlr_egl_get_display(struct wlr_egl *egl);

/**
 * Get the EGL context used by the struct wlr_egl.
 *
 * This is typically used by compositors which need to perform custom OpenGL
 * operations.
 */
EGLContext wlr_egl_get_context(struct wlr_egl *egl);

#endif
