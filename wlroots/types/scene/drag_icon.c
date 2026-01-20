#include <stdlib.h>
#include "wlr/types/wlr_compositor.h"
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_data_device.h>

struct wlr_scene_drag_icon {
	struct wlr_scene_tree *tree;
	struct wlr_scene_tree *surface_tree;
	struct wlr_drag_icon *drag_icon;

	struct wl_listener tree_destroy;
	struct wl_listener drag_icon_surface_commit;
	struct wl_listener drag_icon_destroy;
};

static void drag_icon_handle_surface_commit(struct wl_listener *listener, void *data) {
	struct wlr_scene_drag_icon *icon =
		wl_container_of(listener, icon, drag_icon_surface_commit);
	struct wlr_surface *surface = icon->drag_icon->surface;
	struct wlr_scene_node *node = &icon->surface_tree->node;
	wlr_scene_node_set_position(node,
		node->x + surface->current.dx, node->y + surface->current.dy);
}

static void drag_icon_handle_tree_destroy(struct wl_listener *listener, void *data) {
	struct wlr_scene_drag_icon *icon =
		wl_container_of(listener, icon, tree_destroy);
	wl_list_remove(&icon->tree_destroy.link);
	wl_list_remove(&icon->drag_icon_surface_commit.link);
	wl_list_remove(&icon->drag_icon_destroy.link);
	free(icon);
}

static void drag_icon_handle_destroy(struct wl_listener *listener, void *data) {
	struct wlr_scene_drag_icon *icon =
		wl_container_of(listener, icon, drag_icon_destroy);
	wlr_scene_node_destroy(&icon->tree->node);
}

struct wlr_scene_tree *wlr_scene_drag_icon_create(
		struct wlr_scene_tree *parent, struct wlr_drag_icon *drag_icon) {
	struct wlr_scene_drag_icon *icon = calloc(1, sizeof(*icon));
	if (!icon) {
		return NULL;
	}

	icon->drag_icon = drag_icon;

	icon->tree = wlr_scene_tree_create(parent);
	if (!icon->tree) {
		free(icon);
		return NULL;
	}

	icon->surface_tree = wlr_scene_subsurface_tree_create(
		icon->tree, drag_icon->surface);
	if (!icon->surface_tree) {
		wlr_scene_node_destroy(&icon->tree->node);
		free(icon);
		return NULL;
	}

	icon->tree_destroy.notify = drag_icon_handle_tree_destroy;
	wl_signal_add(&icon->tree->node.events.destroy, &icon->tree_destroy);
	icon->drag_icon_surface_commit.notify = drag_icon_handle_surface_commit;
	wl_signal_add(&drag_icon->surface->events.commit, &icon->drag_icon_surface_commit);
	icon->drag_icon_destroy.notify = drag_icon_handle_destroy;
	wl_signal_add(&drag_icon->events.destroy, &icon->drag_icon_destroy);

	return icon->tree;
}
