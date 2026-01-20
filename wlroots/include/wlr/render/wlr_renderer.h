/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_RENDER_WLR_RENDERER_H
#define WLR_RENDER_WLR_RENDERER_H

#include <stdint.h>
#include <wayland-server-core.h>
#include <wlr/render/pass.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/util/box.h>

struct wlr_backend;
struct wlr_renderer_impl;
struct wlr_drm_format_set;
struct wlr_buffer;
struct wlr_box;
struct wlr_fbox;

/**
 * A renderer for basic 2D operations.
 */
struct wlr_renderer {
	// Capabilities required for the buffer used as a render target (bitmask of
	// enum wlr_buffer_cap)
	uint32_t render_buffer_caps;
	// Supported color encodings for YCbCr textures
	uint32_t color_encodings; // bitmask of enum wlr_color_encoding

	struct {
		struct wl_signal destroy;
		/**
		 * Emitted when the GPU is lost, e.g. on GPU reset.
		 *
		 * Compositors should destroy the renderer and re-create it.
		 */
		struct wl_signal lost;
	} events;

	struct {
		/**
		 * Whether color transforms are supported for input textures
		 */
		bool input_color_transform;
		/**
		 * Does the renderer support color transforms on its output?
		 */
		bool output_color_transform;
		/**
		 * Whether wait/signal timelines are supported.
		 *
		 * See struct wlr_drm_syncobj_timeline.
		 */
		bool timeline;
	} features;

	struct {
		const struct wlr_renderer_impl *impl;
	} WLR_PRIVATE;
};

/**
 * Automatically create a new renderer.
 *
 * Selects an appropriate renderer type to use depending on the backend,
 * platform, environment, etc.
 */
struct wlr_renderer *wlr_renderer_autocreate(struct wlr_backend *backend);

/**
 * Get the formats supporting sampling usage.
 *
 * The buffer capabilities must be passed in.
 *
 * Buffers allocated with a format from this list may be passed to
 * wlr_texture_from_buffer().
 */
const struct wlr_drm_format_set *wlr_renderer_get_texture_formats(
	struct wlr_renderer *r, uint32_t buffer_caps);

/**
 * Initializes wl_shm, linux-dmabuf and other buffer factory protocols.
 *
 * Returns false on failure.
 */
bool wlr_renderer_init_wl_display(struct wlr_renderer *r,
	struct wl_display *wl_display);

/**
 * Initializes wl_shm on the provided struct wl_display.
 */
bool wlr_renderer_init_wl_shm(struct wlr_renderer *r,
	struct wl_display *wl_display);

/**
 * Obtains the FD of the DRM device used for rendering, or -1 if unavailable.
 *
 * The caller doesn't have ownership of the FD, it must not close it.
 */
int wlr_renderer_get_drm_fd(struct wlr_renderer *r);

/**
 * Destroys the renderer.
 *
 * Textures must be destroyed separately.
 */
void wlr_renderer_destroy(struct wlr_renderer *renderer);

/**
 * Allocate and initialise a new render timer.
 */
struct wlr_render_timer *wlr_render_timer_create(struct wlr_renderer *renderer);

/**
 * Get the render duration in nanoseconds from the timer.
 *
 * Returns -1 if the duration is unavailable.
 */
int wlr_render_timer_get_duration_ns(struct wlr_render_timer *timer);

/**
 * Destroy the render timer.
 */
void wlr_render_timer_destroy(struct wlr_render_timer *timer);

#endif
