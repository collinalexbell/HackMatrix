#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <wlr/backend/interface.h>
#include <wlr/interfaces/wlr_ext_image_capture_source_v1.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/types/wlr_ext_image_copy_capture_v1.h>
#include <wlr/util/log.h>

#include "types/wlr_output.h"
#include "types/wlr_scene.h"

struct scene_node_source {
	struct wlr_ext_image_capture_source_v1 base;

	struct wlr_scene_node *node;

	struct wlr_backend backend;
	struct wlr_output output;
	struct wlr_scene_output *scene_output;

	struct wl_event_source *idle_frame;

	struct wl_listener node_destroy;
	struct wl_listener scene_output_destroy;
	struct wl_listener output_frame;
};

struct scene_node_source_frame_event {
	struct wlr_ext_image_capture_source_v1_frame_event base;
	struct wlr_buffer *buffer;
	struct timespec when;
};

static size_t last_output_num = 0;

static void _get_scene_node_extents(struct wlr_scene_node *node, struct wlr_box *box, int lx, int ly) {
	switch (node->type) {
	case WLR_SCENE_NODE_TREE:;
		struct wlr_scene_tree *scene_tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &scene_tree->children, link) {
			_get_scene_node_extents(child, box, lx + child->x, ly + child->y);
		}
		break;
	case WLR_SCENE_NODE_RECT:
	case WLR_SCENE_NODE_BUFFER:;
		struct wlr_box node_box = { .x = lx, .y = ly };
		scene_node_get_size(node, &node_box.width, &node_box.height);

		if (node_box.x < box->x) {
			box->x = node_box.x;
		}
		if (node_box.y < box->y) {
			box->y = node_box.y;
		}
		if (node_box.x + node_box.width > box->x + box->width) {
			box->width = node_box.x + node_box.width - box->x;
		}
		if (node_box.y + node_box.height > box->y + box->height) {
			box->height = node_box.y + node_box.height - box->y;
		}
		break;
	}
}

static void get_scene_node_extents(struct wlr_scene_node *node, struct wlr_box *box) {
	*box = (struct wlr_box){ .x = INT_MAX, .y = INT_MAX };
	int lx = 0, ly = 0;
	wlr_scene_node_coords(node, &lx, &ly);
	_get_scene_node_extents(node, box, lx, ly);
}

static void source_render(struct scene_node_source *source) {
	struct wlr_scene_output *scene_output = source->scene_output;

	struct wlr_box extents;
	get_scene_node_extents(source->node, &extents);

	if (extents.width == 0 || extents.height == 0) {
		return;
	}

	wlr_scene_output_set_position(scene_output, extents.x, extents.y);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, true);
	wlr_output_state_set_custom_mode(&state, extents.width, extents.height, 0);
	bool ok = wlr_scene_output_build_state(scene_output, &state, NULL) &&
		wlr_output_commit_state(scene_output->output, &state);
	wlr_output_state_finish(&state);

	if (!ok) {
		// TODO: send failure
		return;
	}

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_scene_output_send_frame_done(scene_output, &now);
}

static void source_start(struct wlr_ext_image_capture_source_v1 *base, bool with_cursors) {
	struct scene_node_source *source = wl_container_of(base, source, base);
	source_render(source);
}

static void source_stop(struct wlr_ext_image_capture_source_v1 *base) {
	struct scene_node_source *source = wl_container_of(base, source, base);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, false);
	wlr_output_commit_state(&source->output, &state);
	wlr_output_state_finish(&state);
}

static void source_schedule_frame(struct wlr_ext_image_capture_source_v1 *base) {
	struct scene_node_source *source = wl_container_of(base, source, base);
	wlr_output_update_needs_frame(&source->output);
}

static void source_copy_frame(struct wlr_ext_image_capture_source_v1 *base,
		struct wlr_ext_image_copy_capture_frame_v1 *frame,
		struct wlr_ext_image_capture_source_v1_frame_event *base_event) {
	struct scene_node_source *source = wl_container_of(base, source, base);
	struct scene_node_source_frame_event *event = wl_container_of(base_event, event, base);

	if (wlr_ext_image_copy_capture_frame_v1_copy_buffer(frame,
			event->buffer, source->output.renderer)) {
		wlr_ext_image_copy_capture_frame_v1_ready(frame,
			source->output.transform, &event->when);
	}
}

static const struct wlr_ext_image_capture_source_v1_interface source_impl = {
	.start = source_start,
	.stop = source_stop,
	.schedule_frame = source_schedule_frame,
	.copy_frame = source_copy_frame,
};

static const struct wlr_backend_impl backend_impl = {0};

static void source_update_buffer_constraints(struct scene_node_source *source,
		const struct wlr_output_state *state) {
	struct wlr_output *output = &source->output;

	if (!wlr_output_configure_primary_swapchain(output, state, &output->swapchain)) {
		return;
	}

	wlr_ext_image_capture_source_v1_set_constraints_from_swapchain(&source->base,
		output->swapchain, output->renderer);
}

static void source_handle_idle_frame(void *data) {
	struct scene_node_source *source = data;
	source->idle_frame = NULL;
	wlr_output_send_frame(&source->output);
}

static bool output_test(struct wlr_output *output, const struct wlr_output_state *state) {
	struct scene_node_source *source = wl_container_of(output, source, output);

	uint32_t supported =
		WLR_OUTPUT_STATE_BACKEND_OPTIONAL |
		WLR_OUTPUT_STATE_BUFFER |
		WLR_OUTPUT_STATE_ENABLED |
		WLR_OUTPUT_STATE_MODE;
	if ((state->committed & ~supported) != 0) {
		return false;
	}

	if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		int pending_width, pending_height;
		output_pending_resolution(output, state,
			&pending_width, &pending_height);
		if (state->buffer->width != pending_width ||
				state->buffer->height != pending_height) {
			return false;
		}
		struct wlr_fbox src_box;
		output_state_get_buffer_src_box(state, &src_box);
		if (src_box.x != 0.0 || src_box.y != 0.0 ||
				src_box.width != (double)state->buffer->width ||
				src_box.height != (double)state->buffer->height) {
			return false;
		}
	}

	return true;
}

static bool output_commit(struct wlr_output *output, const struct wlr_output_state *state) {
	struct scene_node_source *source = wl_container_of(output, source, output);

	if (source->idle_frame != NULL) {
		wlr_log(WLR_DEBUG, "Failed to commit capture output: a frame is still pending");
		return false;
	}

	if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && !state->enabled) {
		return true;
	}

	if (state->committed & WLR_OUTPUT_STATE_MODE) {
		source_update_buffer_constraints(source, state);
	}

	if (!(state->committed & WLR_OUTPUT_STATE_BUFFER)) {
		wlr_log(WLR_DEBUG, "Failed to commit capture output: missing buffer");
		return false;
	}

	struct wlr_buffer *buffer = state->buffer;

	pixman_region32_t full_damage;
	pixman_region32_init_rect(&full_damage, 0, 0, buffer->width, buffer->height);

	const pixman_region32_t *damage;
	if (state->committed & WLR_OUTPUT_STATE_DAMAGE) {
		damage = &state->damage;
	} else {
		damage = &full_damage;
	}

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);

	struct scene_node_source_frame_event frame_event = {
		.base = { .damage = damage },
		.buffer = buffer,
		.when = now,
	};
	wl_signal_emit_mutable(&source->base.events.frame, &frame_event.base);

	pixman_region32_fini(&full_damage);

	source->idle_frame =
		wl_event_loop_add_idle(output->event_loop, source_handle_idle_frame, source);

	return true;
}

static const struct wlr_output_impl output_impl = {
	.test = output_test,
	.commit = output_commit,
};

static void source_destroy(struct scene_node_source *source) {
	wl_list_remove(&source->node_destroy.link);
	wl_list_remove(&source->scene_output_destroy.link);
	wl_list_remove(&source->output_frame.link);
	wlr_ext_image_capture_source_v1_finish(&source->base);
	wlr_scene_output_destroy(source->scene_output);
	wlr_output_finish(&source->output);
	wlr_backend_finish(&source->backend);
	free(source);
}

static void source_handle_node_destroy(struct wl_listener *listener, void *data) {
	struct scene_node_source *source = wl_container_of(listener, source, node_destroy);
	source_destroy(source);
}

static void source_handle_scene_output_destroy(struct wl_listener *listener, void *data) {
	struct scene_node_source *source = wl_container_of(listener, source, scene_output_destroy);
	source->scene_output = NULL;
	wl_list_remove(&source->scene_output_destroy.link);
	wl_list_init(&source->scene_output_destroy.link);
}

static void source_handle_output_frame(struct wl_listener *listener, void *data) {
	struct scene_node_source *source = wl_container_of(listener, source, output_frame);
	if (source->scene_output == NULL) {
		return;
	}

	if (!wlr_scene_output_needs_frame(source->scene_output)) {
		return;
	}

	source_render(source);
}

struct wlr_ext_image_capture_source_v1 *wlr_ext_image_capture_source_v1_create_with_scene_node(
		struct wlr_scene_node *node, struct wl_event_loop *event_loop,
		struct wlr_allocator *allocator, struct wlr_renderer *renderer) {
	struct scene_node_source *source = calloc(1, sizeof(*source));
	if (source == NULL) {
		return NULL;
	}

	source->node = node;

	wlr_ext_image_capture_source_v1_init(&source->base, &source_impl);

	wlr_backend_init(&source->backend, &backend_impl);
	source->backend.buffer_caps = WLR_BUFFER_CAP_DMABUF | WLR_BUFFER_CAP_SHM;

	wlr_output_init(&source->output, &source->backend, &output_impl, event_loop, NULL);

	size_t output_num = ++last_output_num;
	char name[64];
	snprintf(name, sizeof(name), "CAPTURE-%zu", output_num);
	wlr_output_set_name(&source->output, name);

	wlr_output_init_render(&source->output, allocator, renderer);

	struct wlr_scene *scene = scene_node_get_root(node);
	source->scene_output = wlr_scene_output_create(scene, &source->output);

	source->node_destroy.notify = source_handle_node_destroy;
	wl_signal_add(&node->events.destroy, &source->node_destroy);

	source->scene_output_destroy.notify = source_handle_scene_output_destroy;
	wl_signal_add(&source->scene_output->events.destroy, &source->scene_output_destroy);

	source->output_frame.notify = source_handle_output_frame;
	wl_signal_add(&source->output.events.frame, &source->output_frame);

	return &source->base;
}
