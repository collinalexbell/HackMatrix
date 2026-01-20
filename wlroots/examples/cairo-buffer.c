
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <cairo.h>
#include <drm_fourcc.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include <wlr/interfaces/wlr_buffer.h>

/* Simple scene-graph example with a custom buffer drawn by Cairo.
 *
 * Input is unimplemented. Surfaces are unimplemented. */

struct cairo_buffer {
	struct wlr_buffer base;
	cairo_surface_t *surface;
};

static void cairo_buffer_destroy(struct wlr_buffer *wlr_buffer) {
	struct cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);
	wlr_buffer_finish(wlr_buffer);
	cairo_surface_destroy(buffer->surface);
	free(buffer);
}

static bool cairo_buffer_begin_data_ptr_access(struct wlr_buffer *wlr_buffer,
		uint32_t flags, void **data, uint32_t *format, size_t *stride) {
	struct cairo_buffer *buffer = wl_container_of(wlr_buffer, buffer, base);

	if (flags & WLR_BUFFER_DATA_PTR_ACCESS_WRITE) {
		return false;
	}

	*format = DRM_FORMAT_ARGB8888;
	*data = cairo_image_surface_get_data(buffer->surface);
	*stride = cairo_image_surface_get_stride(buffer->surface);
	return true;
}

static void cairo_buffer_end_data_ptr_access(struct wlr_buffer *wlr_buffer) {
}

static const struct wlr_buffer_impl cairo_buffer_impl = {
	.destroy = cairo_buffer_destroy,
	.begin_data_ptr_access = cairo_buffer_begin_data_ptr_access,
	.end_data_ptr_access = cairo_buffer_end_data_ptr_access
};

static struct cairo_buffer *create_cairo_buffer(int width, int height) {
	struct cairo_buffer *buffer = calloc(1, sizeof(*buffer));
	if (!buffer) {
		return NULL;
	}

	wlr_buffer_init(&buffer->base, &cairo_buffer_impl, width, height);

	buffer->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
			width, height);
	if (cairo_surface_status(buffer->surface) != CAIRO_STATUS_SUCCESS) {
		free(buffer);
		return NULL;
	}

	return buffer;
}

struct server {
	struct wl_display *display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_scene *scene;

	struct wl_listener new_output;
};

struct output {
	struct wl_list link;
	struct server *server;
	struct wlr_output *wlr;
	struct wlr_scene_output *scene_output;

	struct wl_listener frame;
};

static void output_handle_frame(struct wl_listener *listener, void *data) {
	struct output *output = wl_container_of(listener, output, frame);

	wlr_scene_output_commit(output->scene_output, NULL);
}

static void server_handle_new_output(struct wl_listener *listener, void *data) {
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	struct output *output = calloc(1, sizeof(*output));
	output->wlr = wlr_output;
	output->server = server;
	output->frame.notify = output_handle_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	output->scene_output = wlr_scene_output_create(server->scene, wlr_output);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);
}

int main(void) {
	wlr_log_init(WLR_DEBUG, NULL);

	struct server server = {0};
	server.display = wl_display_create();
	server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.display), NULL);
	server.scene = wlr_scene_create();

	server.renderer = wlr_renderer_autocreate(server.backend);
	wlr_renderer_init_wl_display(server.renderer, server.display);

	server.allocator = wlr_allocator_autocreate(server.backend,
			server.renderer);

	server.new_output.notify = server_handle_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	if (!wlr_backend_start(server.backend)) {
		wl_display_destroy(server.display);
		return EXIT_FAILURE;
	}

	struct cairo_buffer *buffer = create_cairo_buffer(256, 256);
	if (!buffer) {
		wl_display_destroy(server.display);
		return EXIT_FAILURE;
	}

	/* Begin drawing
	 * From cairo samples at https://www.cairographics.org/samples/ */
	cairo_t *cr = cairo_create(buffer->surface);
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_paint(cr);
	cairo_set_source_rgb(cr, 0, 0, 0);

	double x = 25.6, y = 128.0;
	double x1 = 102.4, y1 = 230.4,
			x2 = 153.6, y2 = 25.6,
			x3 = 230.4, y3 = 128.0;

	cairo_move_to(cr, x, y);
	cairo_curve_to(cr, x1, y1, x2, y2, x3, y3);

	cairo_set_line_width(cr, 10.0);
	cairo_stroke(cr);

	cairo_set_source_rgba(cr, 1, 0.2, 0.2, 0.6);
	cairo_set_line_width(cr, 6.0);
	cairo_move_to(cr, x, y);
	cairo_line_to(cr, x1, y1);
	cairo_move_to(cr, x2, y2);
	cairo_line_to(cr, x3, y3);
	cairo_stroke(cr);

	cairo_destroy(cr);
	/* End drawing */

	struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_create(
			&server.scene->tree, &buffer->base);
	if (!scene_buffer) {
		wl_display_destroy(server.display);
		return EXIT_FAILURE;
	}

	wlr_scene_node_set_position(&scene_buffer->node, 50, 50);
	wlr_buffer_drop(&buffer->base);

	wl_display_run(server.display);

	wl_display_destroy(server.display);
	return EXIT_SUCCESS;
}
