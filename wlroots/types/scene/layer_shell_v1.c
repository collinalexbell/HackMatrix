#include <stdlib.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_layer_shell_v1.h>

static void scene_layer_surface_handle_tree_destroy(
		struct wl_listener *listener, void *data) {
	struct wlr_scene_layer_surface_v1 *scene_layer_surface =
		wl_container_of(listener, scene_layer_surface, tree_destroy);
	// tree and surface_node will be cleaned up by scene_node_finish
	wl_list_remove(&scene_layer_surface->tree_destroy.link);
	wl_list_remove(&scene_layer_surface->layer_surface_destroy.link);
	free(scene_layer_surface);
}

static void scene_layer_surface_handle_layer_surface_destroy(
		struct wl_listener *listener, void *data) {
	struct wlr_scene_layer_surface_v1 *scene_layer_surface =
		wl_container_of(listener, scene_layer_surface, layer_surface_destroy);
	wlr_scene_node_destroy(&scene_layer_surface->tree->node);
}

static void layer_surface_exclusive_zone(
		struct wlr_layer_surface_v1_state *state,
		struct wlr_box *usable_area) {
	switch (state->anchor) {
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP:
	case (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT):
		// Anchor top
		usable_area->y += state->exclusive_zone + state->margin.top;
		usable_area->height -= state->exclusive_zone + state->margin.top;
		break;
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM:
	case (ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT):
		// Anchor bottom
		usable_area->height -= state->exclusive_zone + state->margin.bottom;
		break;
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT:
	case (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT):
		// Anchor left
		usable_area->x += state->exclusive_zone + state->margin.left;
		usable_area->width -= state->exclusive_zone + state->margin.left;
		break;
	case ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT:
	case (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT):
		// Anchor right
		usable_area->width -= state->exclusive_zone + state->margin.right;
		break;
	}

	if (usable_area->width < 0) {
		usable_area->width = 0;
	}
	if (usable_area->height < 0) {
		usable_area->height = 0;
	}
}

void wlr_scene_layer_surface_v1_configure(
		struct wlr_scene_layer_surface_v1 *scene_layer_surface,
		const struct wlr_box *full_area, struct wlr_box *usable_area) {
	struct wlr_layer_surface_v1 *layer_surface =
		scene_layer_surface->layer_surface;
	struct wlr_layer_surface_v1_state *state = &layer_surface->current;

	// If the exclusive zone is set to -1, the layer surface will use the
	// full area of the output, otherwise it is constrained to the
	// remaining usable area.
	struct wlr_box bounds;
	if (state->exclusive_zone == -1) {
		bounds = *full_area;
	} else {
		bounds = *usable_area;
	}

	struct wlr_box box = {
		.width = state->desired_width,
		.height = state->desired_height,
	};

	// Horizontal positioning
	if (box.width == 0) {
		box.x = bounds.x + state->margin.left;
		box.width = bounds.width -
			(state->margin.left + state->margin.right);
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT &&
			state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
		box.x = bounds.x + bounds.width/2 -box.width/2;
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
		box.x = bounds.x + state->margin.left;
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
		box.x = bounds.x + bounds.width - box.width - state->margin.right;
	} else {
		box.x = bounds.x + bounds.width/2 - box.width/2;
	}

	// Vertical positioning
	if (box.height == 0) {
		box.y = bounds.y + state->margin.top;
		box.height = bounds.height -
			(state->margin.top + state->margin.bottom);
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP &&
			state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
		box.y = bounds.y + bounds.height/2 - box.height/2;
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
		box.y = bounds.y + state->margin.top;
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
		box.y = bounds.y + bounds.height - box.height - state->margin.bottom;
	} else {
		box.y = bounds.y + bounds.height/2 - box.height/2;
	}

	wlr_scene_node_set_position(&scene_layer_surface->tree->node, box.x, box.y);
	wlr_layer_surface_v1_configure(layer_surface, box.width, box.height);

	if (layer_surface->surface->mapped && state->exclusive_zone > 0) {
		layer_surface_exclusive_zone(state, usable_area);
	}
}

struct wlr_scene_layer_surface_v1 *wlr_scene_layer_surface_v1_create(
		struct wlr_scene_tree *parent,
		struct wlr_layer_surface_v1 *layer_surface) {
	struct wlr_scene_layer_surface_v1 *scene_layer_surface =
		calloc(1, sizeof(*scene_layer_surface));
	if (scene_layer_surface == NULL) {
		return NULL;
	}

	scene_layer_surface->layer_surface = layer_surface;

	scene_layer_surface->tree = wlr_scene_tree_create(parent);
	if (scene_layer_surface->tree == NULL) {
		free(scene_layer_surface);
		return NULL;
	}

	struct wlr_scene_tree *surface_tree = wlr_scene_subsurface_tree_create(
		scene_layer_surface->tree, layer_surface->surface);
	if (surface_tree == NULL) {
		wlr_scene_node_destroy(&scene_layer_surface->tree->node);
		free(scene_layer_surface);
		return NULL;
	}

	scene_layer_surface->tree_destroy.notify =
		scene_layer_surface_handle_tree_destroy;
	wl_signal_add(&scene_layer_surface->tree->node.events.destroy,
		&scene_layer_surface->tree_destroy);

	scene_layer_surface->layer_surface_destroy.notify =
		scene_layer_surface_handle_layer_surface_destroy;
	wl_signal_add(&layer_surface->events.destroy,
		&scene_layer_surface->layer_surface_destroy);

	return scene_layer_surface;
}
