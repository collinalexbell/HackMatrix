/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_RENDER_INTERFACE_H
#define WLR_RENDER_INTERFACE_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/wlr_texture.h>
#include <wlr/types/wlr_output.h>
#include <wlr/render/dmabuf.h>

struct wlr_box;
struct wlr_fbox;

struct wlr_renderer_impl {
	const struct wlr_drm_format_set *(*get_texture_formats)(
		struct wlr_renderer *renderer, uint32_t buffer_caps);
	const struct wlr_drm_format_set *(*get_render_formats)(
		struct wlr_renderer *renderer);
	void (*destroy)(struct wlr_renderer *renderer);
	int (*get_drm_fd)(struct wlr_renderer *renderer);
	struct wlr_texture *(*texture_from_buffer)(struct wlr_renderer *renderer,
		struct wlr_buffer *buffer);
	struct wlr_render_pass *(*begin_buffer_pass)(struct wlr_renderer *renderer,
		struct wlr_buffer *buffer, const struct wlr_buffer_pass_options *options);
	struct wlr_render_timer *(*render_timer_create)(struct wlr_renderer *renderer);
};

void wlr_renderer_init(struct wlr_renderer *renderer,
	const struct wlr_renderer_impl *impl, uint32_t render_buffer_caps);

struct wlr_texture_impl {
	bool (*update_from_buffer)(struct wlr_texture *texture,
		struct wlr_buffer *buffer, const pixman_region32_t *damage);
	bool (*read_pixels)(struct wlr_texture *texture,
		const struct wlr_texture_read_pixels_options *options);
	uint32_t (*preferred_read_format)(struct wlr_texture *texture);
	void (*destroy)(struct wlr_texture *texture);
};

void wlr_texture_init(struct wlr_texture *texture, struct wlr_renderer *rendener,
	const struct wlr_texture_impl *impl, uint32_t width, uint32_t height);

struct wlr_render_pass {
	const struct wlr_render_pass_impl *impl;
};

void wlr_render_pass_init(struct wlr_render_pass *pass,
	const struct wlr_render_pass_impl *impl);

struct wlr_render_pass_impl {
	bool (*submit)(struct wlr_render_pass *pass);
	void (*add_texture)(struct wlr_render_pass *pass,
		const struct wlr_render_texture_options *options);
	/* Implementers are also guaranteed that options->box is nonempty */
	void (*add_rect)(struct wlr_render_pass *pass,
		const struct wlr_render_rect_options *options);
};

struct wlr_render_timer {
	const struct wlr_render_timer_impl *impl;
};

struct wlr_render_timer_impl {
	int (*get_duration_ns)(struct wlr_render_timer *timer);
	void (*destroy)(struct wlr_render_timer *timer);
};

void wlr_render_texture_options_get_src_box(const struct wlr_render_texture_options *options,
	struct wlr_fbox *box);
void wlr_render_texture_options_get_dst_box(const struct wlr_render_texture_options *options,
	struct wlr_box *box);
float wlr_render_texture_options_get_alpha(const struct wlr_render_texture_options *options);
void wlr_render_rect_options_get_box(const struct wlr_render_rect_options *options,
	const struct wlr_buffer *buffer, struct wlr_box *box);

void wlr_texture_read_pixels_options_get_src_box(
	const struct wlr_texture_read_pixels_options *options,
	const struct wlr_texture *texture, struct wlr_box *box);
void *wlr_texture_read_pixel_options_get_data(
	const struct wlr_texture_read_pixels_options *options);

#endif
