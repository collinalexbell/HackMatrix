#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layer.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>

/* Simple compositor making use of the output layers API. The compositor will
 * attempt to display client surfaces with output layers. Input is
 * unimplemented.
 *
 * New surfaces are stacked on top of the existing ones as they appear.
 * Surfaces that don't make it into an output layer are rendered as usual. */

struct server {
	struct wl_display *wl_display;
	struct wlr_backend *backend;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1;

	struct wl_list outputs;

	struct wl_listener new_output;
	struct wl_listener new_surface;
};

struct output_surface {
	struct wlr_surface *wlr_surface;
	struct wlr_output_layer *layer;
	struct server *server;
	struct wl_list link;

	int x, y;
	struct wlr_buffer *buffer;

	bool first_commit, layer_accepted, prev_layer_accepted;

	struct wl_listener destroy;
	struct wl_listener commit;
	struct wl_listener layer_feedback;
};

struct output {
	struct wl_list link;
	struct server *server;
	struct wlr_output *wlr_output;
	struct wl_list surfaces;

	struct wl_listener frame;
};

static void output_handle_frame(struct wl_listener *listener, void *data) {
	struct output *output = wl_container_of(listener, output, frame);

	struct wl_array layers_arr = {0};
	struct output_surface *output_surface;
	wl_list_for_each(output_surface, &output->surfaces, link) {
		struct wlr_output_layer_state *layer_state =
			wl_array_add(&layers_arr, sizeof(*layer_state));
		*layer_state = (struct wlr_output_layer_state){
			.layer = output_surface->layer,
			.buffer = output_surface->buffer,
			.dst_box = {
				.x = output_surface->x,
				.y = output_surface->y,
				.width = output_surface->wlr_surface->current.width,
				.height = output_surface->wlr_surface->current.height,
			},
		};
	}

	struct wlr_output_state output_state;
	wlr_output_state_init(&output_state);
	wlr_output_state_set_layers(&output_state, layers_arr.data,
		layers_arr.size / sizeof(struct wlr_output_layer_state));

	if (!wlr_output_test_state(output->wlr_output, &output_state)) {
		wlr_log(WLR_ERROR, "wlr_output_test_state() failed");
		return;
	}

	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);

	struct wlr_render_pass *pass = wlr_output_begin_render_pass(output->wlr_output, &output_state, NULL);
	wlr_render_pass_add_rect(pass, &(struct wlr_render_rect_options){
		.box = { .width = width, .height = height },
		.color = { 0.3, 0.3, 0.3, 1 },
	});

	size_t i = 0;
	struct wlr_output_layer_state *layers = layers_arr.data;
	wl_list_for_each(output_surface, &output->surfaces, link) {
		struct wlr_surface *wlr_surface = output_surface->wlr_surface;

		output_surface->layer_accepted = layers[i].accepted;
		i++;

		if (wlr_surface->buffer == NULL || output_surface->layer_accepted) {
			continue;
		}

		struct wlr_texture *texture = wlr_surface_get_texture(wlr_surface);
		if (texture == NULL) {
			continue;
		}

		wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options){
			.texture = texture,
			.dst_box = { .x = output_surface->x, .y = output_surface->y },
		});
	}

	wlr_render_pass_submit(pass);

	wlr_output_commit_state(output->wlr_output, &output_state);
	wlr_output_state_finish(&output_state);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	wl_list_for_each(output_surface, &output->surfaces, link) {
		wlr_surface_send_frame_done(output_surface->wlr_surface, &now);

		if (output_surface->wlr_surface->buffer == NULL) {
			continue;
		}

		if ((output_surface->first_commit ||
				!output_surface->prev_layer_accepted) &&
				output_surface->layer_accepted) {
			wlr_log(WLR_INFO, "Scanning out wlr_surface %p on output '%s'",
				output_surface->wlr_surface, output->wlr_output->name);
		}
		if ((output_surface->first_commit ||
				output_surface->prev_layer_accepted) &&
				!output_surface->layer_accepted) {
			wlr_log(WLR_INFO, "Cannot scan out wlr_surface %p on output '%s'",
				output_surface->wlr_surface, output->wlr_output->name);
		}
		output_surface->prev_layer_accepted = output_surface->layer_accepted;
		output_surface->first_commit = false;
	}

	wl_array_release(&layers_arr);
}

static void server_handle_new_output(struct wl_listener *listener, void *data) {
	struct server *server = wl_container_of(listener, server, new_output);
	struct wlr_output *wlr_output = data;

	wlr_output_init_render(wlr_output, server->allocator, server->renderer);

	struct output *output = calloc(1, sizeof(*output));
	output->wlr_output = wlr_output;
	output->server = server;
	wl_list_init(&output->surfaces);
	output->frame.notify = output_handle_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);
	wl_list_insert(&server->outputs, &output->link);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
	if (mode != NULL) {
		wlr_output_state_set_mode(&state, mode);
	}
	wlr_output_commit_state(wlr_output, &state);
	wlr_output_state_finish(&state);

	wlr_output_create_global(wlr_output, server->wl_display);
}

static void output_surface_handle_destroy(struct wl_listener *listener,
		void *data) {
	struct output_surface *output_surface =
		wl_container_of(listener, output_surface, destroy);
	wlr_buffer_unlock(output_surface->buffer);
	wl_list_remove(&output_surface->destroy.link);
	wl_list_remove(&output_surface->commit.link);
	wl_list_remove(&output_surface->layer_feedback.link);
	wl_list_remove(&output_surface->link);
	wlr_output_layer_destroy(output_surface->layer);
	free(output_surface);
}

static void output_surface_handle_commit(struct wl_listener *listener,
		void *data) {
	struct output_surface *output_surface =
		wl_container_of(listener, output_surface, commit);

	struct wlr_xdg_toplevel *xdg_toplevel =
		wlr_xdg_toplevel_try_from_wlr_surface(output_surface->wlr_surface);
	if (xdg_toplevel != NULL && xdg_toplevel->base->initial_commit) {
		wlr_xdg_toplevel_set_size(xdg_toplevel, 0, 0);
	}

	struct wlr_buffer *buffer = NULL;
	if (output_surface->wlr_surface->buffer != NULL) {
		buffer = wlr_buffer_lock(&output_surface->wlr_surface->buffer->base);
	}

	wlr_buffer_unlock(output_surface->buffer);
	output_surface->buffer = buffer;
}

static void output_surface_handle_layer_feedback(struct wl_listener *listener,
		void *data) {
	struct output_surface *output_surface =
		wl_container_of(listener, output_surface, layer_feedback);
	const struct wlr_output_layer_feedback_event *event = data;

	wlr_log(WLR_DEBUG, "Sending linux-dmabuf feedback to surface %p",
		output_surface->wlr_surface);

	struct wlr_linux_dmabuf_feedback_v1 feedback = {0};
	const struct wlr_linux_dmabuf_feedback_v1_init_options options = {
		.main_renderer = output_surface->server->renderer,
		.output_layer_feedback_event = event,
	};
	wlr_linux_dmabuf_feedback_v1_init_with_options(&feedback, &options);
	wlr_linux_dmabuf_v1_set_surface_feedback(output_surface->server->linux_dmabuf_v1,
		output_surface->wlr_surface, &feedback);
	wlr_linux_dmabuf_feedback_v1_finish(&feedback);
}

static void server_handle_new_surface(struct wl_listener *listener,
		void *data) {
	struct server *server = wl_container_of(listener, server, new_surface);
	struct wlr_surface *wlr_surface = data;

	struct output *output;
	wl_list_for_each(output, &server->outputs, link) {
		struct output_surface *output_surface = calloc(1, sizeof(*output_surface));
		output_surface->wlr_surface = wlr_surface;
		output_surface->server = server;
		output_surface->destroy.notify = output_surface_handle_destroy;
		wl_signal_add(&wlr_surface->events.destroy, &output_surface->destroy);
		output_surface->commit.notify = output_surface_handle_commit;
		wl_signal_add(&wlr_surface->events.commit, &output_surface->commit);

		output_surface->layer = wlr_output_layer_create(output->wlr_output);
		output_surface->layer_feedback.notify = output_surface_handle_layer_feedback;
		wl_signal_add(&output_surface->layer->events.feedback,
			&output_surface->layer_feedback);

		int pos = 50 * wl_list_length(&output->surfaces);

		output_surface->x = output_surface->y = pos;
		output_surface->first_commit = true;

		wl_list_insert(output->surfaces.prev, &output_surface->link);
	}
}

int main(int argc, char *argv[]) {
	wlr_log_init(WLR_DEBUG, NULL);

	char *startup_cmd = NULL;

	int c;
	while ((c = getopt(argc, argv, "s:")) != -1) {
		switch (c) {
		case 's':
			startup_cmd = optarg;
			break;
		default:
			printf("usage: %s [-s startup-command]\n", argv[0]);
			return EXIT_FAILURE;
		}
	}
	if (optind < argc) {
		printf("usage: %s [-s startup-command]\n", argv[0]);
		return EXIT_FAILURE;
	}

	struct server server = {0};
	server.wl_display = wl_display_create();
	server.backend = wlr_backend_autocreate(wl_display_get_event_loop(server.wl_display), NULL);

	server.renderer = wlr_renderer_autocreate(server.backend);
	wlr_renderer_init_wl_shm(server.renderer, server.wl_display);

	if (wlr_renderer_get_texture_formats(server.renderer, WLR_BUFFER_CAP_DMABUF) != NULL) {
		wlr_drm_create(server.wl_display, server.renderer);
		server.linux_dmabuf_v1 = wlr_linux_dmabuf_v1_create_with_renderer(
			server.wl_display, 4, server.renderer);
	}

	server.allocator = wlr_allocator_autocreate(server.backend,
		server.renderer);

	struct wlr_compositor *compositor =
		wlr_compositor_create(server.wl_display, 5, server.renderer);

	wlr_xdg_shell_create(server.wl_display, 1);

	wl_list_init(&server.outputs);

	server.new_output.notify = server_handle_new_output;
	wl_signal_add(&server.backend->events.new_output, &server.new_output);

	server.new_surface.notify = server_handle_new_surface;
	wl_signal_add(&compositor->events.new_surface, &server.new_surface);

	const char *socket = wl_display_add_socket_auto(server.wl_display);
	if (!socket) {
		wl_display_destroy(server.wl_display);
		return EXIT_FAILURE;
	}

	if (!wlr_backend_start(server.backend)) {
		wl_display_destroy(server.wl_display);
		return EXIT_FAILURE;
	}

	setenv("WAYLAND_DISPLAY", socket, true);
	if (startup_cmd != NULL) {
		if (fork() == 0) {
			execlp("sh", "sh", "-c", startup_cmd, (char *)NULL);
		}
	}

	wlr_log(WLR_INFO, "Running Wayland compositor on WAYLAND_DISPLAY=%s",
		socket);
	wl_display_run(server.wl_display);

	wl_display_destroy_clients(server.wl_display);
	wl_display_destroy(server.wl_display);
	return EXIT_SUCCESS;
}
