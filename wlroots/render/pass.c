#include <assert.h>
#include <string.h>
#include <wlr/render/interface.h>

void wlr_render_pass_init(struct wlr_render_pass *render_pass,
		const struct wlr_render_pass_impl *impl) {
	assert(impl->submit && impl->add_texture && impl->add_rect);
	*render_pass = (struct wlr_render_pass){
		.impl = impl,
	};
}

bool wlr_render_pass_submit(struct wlr_render_pass *render_pass) {
	return render_pass->impl->submit(render_pass);
}

void wlr_render_pass_add_texture(struct wlr_render_pass *render_pass,
		const struct wlr_render_texture_options *options) {
	// make sure the texture source box does not try and sample outside of the
	// texture
	if (!wlr_fbox_empty(&options->src_box)) {
		const struct wlr_fbox *box = &options->src_box;
		assert(box->x >= 0 && box->y >= 0 &&
		(uint32_t)(box->x + box->width) <= options->texture->width &&
		(uint32_t)(box->y + box->height) <= options->texture->height);
	}

	render_pass->impl->add_texture(render_pass, options);
}

void wlr_render_pass_add_rect(struct wlr_render_pass *render_pass,
		const struct wlr_render_rect_options *options) {
	assert(options->box.width >= 0 && options->box.height >= 0);
	render_pass->impl->add_rect(render_pass, options);
}

void wlr_render_texture_options_get_src_box(const struct wlr_render_texture_options *options,
		struct wlr_fbox *box) {
	*box = options->src_box;
	if (wlr_fbox_empty(box)) {
		*box = (struct wlr_fbox){
			.width = options->texture->width,
			.height = options->texture->height,
		};
	}
}

void wlr_render_texture_options_get_dst_box(const struct wlr_render_texture_options *options,
		struct wlr_box *box) {
	*box = options->dst_box;
	if (wlr_box_empty(box)) {
		box->width = options->texture->width;
		box->height = options->texture->height;
	}
}

float wlr_render_texture_options_get_alpha(const struct wlr_render_texture_options *options) {
	if (options->alpha == NULL) {
		return 1;
	}
	return *options->alpha;
}

void wlr_render_rect_options_get_box(const struct wlr_render_rect_options *options,
		const struct wlr_buffer *buffer, struct wlr_box *box) {
	if (wlr_box_empty(&options->box)) {
		*box = (struct wlr_box){
			.width = buffer->width,
			.height = buffer->height,
		};

		return;
	}

	*box = options->box;
}
