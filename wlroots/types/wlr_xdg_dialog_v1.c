#include <assert.h>
#include <stdlib.h>

#include <wlr/types/wlr_xdg_dialog_v1.h>
#include <wlr/types/wlr_xdg_shell.h>

#include "xdg-dialog-v1-protocol.h"

// NOTE: xdg_dialog_v1 becomes inert when the corresponding xdg_toplevel is destroyed

#define XDG_WM_DIALOG_V1_VERSION 1

static const struct xdg_dialog_v1_interface dialog_impl;

static struct wlr_xdg_dialog_v1 *dialog_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &xdg_dialog_v1_interface, &dialog_impl));
	return wl_resource_get_user_data(resource);
}

static const struct xdg_wm_dialog_v1_interface wm_impl;

static struct wlr_xdg_wm_dialog_v1 *wm_from_resource(struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource, &xdg_wm_dialog_v1_interface, &wm_impl));
	return wl_resource_get_user_data(resource);
}

static void resource_destroy(struct wl_client *client, struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void surface_addon_destroy(struct wlr_addon *addon) {
	// As wlr_xdg_toplevel is always destroyed before the surface, this should
	// never be reached.
	abort();
}

static const struct wlr_addon_interface surface_addon_impl = {
	.name = "xdg_wm_dialog_v1",
	.destroy = surface_addon_destroy,
};

static void set_modal(struct wlr_xdg_dialog_v1 *dialog, bool modal) {
	if (dialog->modal == modal) {
		return;
	}
	dialog->modal = modal;
	wl_signal_emit_mutable(&dialog->events.set_modal, NULL);
}

static void dialog_handle_set_modal(struct wl_client *client, struct wl_resource *resource) {
	struct wlr_xdg_dialog_v1 *dialog = dialog_from_resource(resource);
	if (dialog == NULL) {
		return;
	}
	set_modal(dialog, true);
}

static void dialog_handle_unset_modal(struct wl_client *client, struct wl_resource *resource) {
	struct wlr_xdg_dialog_v1 *dialog = dialog_from_resource(resource);
	if (dialog == NULL) {
		return;
	}
	set_modal(dialog, false);
};

static const struct xdg_dialog_v1_interface dialog_impl = {
	.destroy = resource_destroy,
	.set_modal = dialog_handle_set_modal,
	.unset_modal = dialog_handle_unset_modal,
};

static void dialog_destroy(struct wlr_xdg_dialog_v1 *dialog) {
	if (dialog == NULL) {
		return;
	}

	wl_signal_emit_mutable(&dialog->events.destroy, NULL);

	assert(wl_list_empty(&dialog->events.destroy.listener_list));
	assert(wl_list_empty(&dialog->events.set_modal.listener_list));

	wlr_addon_finish(&dialog->surface_addon);
	wl_list_remove(&dialog->xdg_toplevel_destroy.link);

	wl_resource_set_user_data(dialog->resource, NULL);
	free(dialog);
}

static void handle_xdg_toplevel_destroy(struct wl_listener *listener, void *data) {
	struct wlr_xdg_dialog_v1 *dialog = wl_container_of(listener, dialog, xdg_toplevel_destroy);
	dialog_destroy(dialog);
}

static void handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_xdg_dialog_v1 *dialog = dialog_from_resource(resource);
	dialog_destroy(dialog);
}

static void wm_get_xdg_dialog(struct wl_client *client, struct wl_resource *wm_resource,
		uint32_t id, struct wl_resource *toplevel_resource) {
	struct wlr_xdg_wm_dialog_v1 *wm = wm_from_resource(wm_resource);
	struct wlr_xdg_toplevel *xdg_toplevel = wlr_xdg_toplevel_from_resource(toplevel_resource);

	struct wlr_addon_set *addon_set = &xdg_toplevel->base->surface->addons;

	if (wlr_addon_find(addon_set, NULL, &surface_addon_impl) != NULL) {
		wl_resource_post_error(wm_resource, XDG_WM_DIALOG_V1_ERROR_ALREADY_USED,
			"the xdg_toplevel object has already been used to create a xdg_dialog_v1");
		return;
	}

	struct wlr_xdg_dialog_v1 *dialog = calloc(1, sizeof(*dialog));
	if (dialog == NULL) {
		wl_resource_post_no_memory(wm_resource);
		return;
	}

	dialog->resource = wl_resource_create(client, &xdg_dialog_v1_interface,
		wl_resource_get_version(wm_resource), id);
	if (dialog->resource == NULL) {
		free(dialog);
		wl_resource_post_no_memory(wm_resource);
		return;
	}
	wl_resource_set_implementation(dialog->resource, &dialog_impl,
		dialog, handle_resource_destroy);

	dialog->xdg_toplevel = xdg_toplevel;
	wlr_addon_init(&dialog->surface_addon, addon_set, NULL, &surface_addon_impl);

	dialog->xdg_toplevel_destroy.notify = handle_xdg_toplevel_destroy;
	wl_signal_add(&xdg_toplevel->events.destroy, &dialog->xdg_toplevel_destroy);

	wl_signal_init(&dialog->events.destroy);
	wl_signal_init(&dialog->events.set_modal);

	wl_signal_emit_mutable(&wm->events.new_dialog, dialog);
}

static const struct xdg_wm_dialog_v1_interface wm_impl = {
	.destroy = resource_destroy,
	.get_xdg_dialog = wm_get_xdg_dialog,
};

static void wm_bind(struct wl_client *client, void *data, uint32_t version, uint32_t id) {
	struct wlr_xdg_wm_dialog_v1 *wm = data;
	struct wl_resource *resource =
		wl_resource_create(client, &xdg_wm_dialog_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &wm_impl, wm, NULL);
}

static void xdg_wm_dialog_handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_xdg_wm_dialog_v1 *wm = wl_container_of(listener, wm, display_destroy);
	wl_signal_emit_mutable(&wm->events.destroy, NULL);

	assert(wl_list_empty(&wm->events.destroy.listener_list));
	assert(wl_list_empty(&wm->events.new_dialog.listener_list));

	wl_list_remove(&wm->display_destroy.link);
	wl_global_destroy(wm->global);
	free(wm);
}

struct wlr_xdg_dialog_v1 *wlr_xdg_dialog_v1_try_from_wlr_xdg_toplevel(
		struct wlr_xdg_toplevel *xdg_toplevel) {
	struct wlr_addon *addon =
		wlr_addon_find(&xdg_toplevel->base->surface->addons, NULL, &surface_addon_impl);
	if (addon == NULL) {
		return NULL;
	}
	struct wlr_xdg_dialog_v1 *dialog = wl_container_of(addon, dialog, surface_addon);
	return dialog;
}

struct wlr_xdg_wm_dialog_v1 *wlr_xdg_wm_dialog_v1_create(struct wl_display *display,
		uint32_t version) {
	assert(version <= XDG_WM_DIALOG_V1_VERSION);

	struct wlr_xdg_wm_dialog_v1 *wm = calloc(1, sizeof(*wm));
	if (wm == NULL) {
		return NULL;
	}

	wm->global = wl_global_create(display, &xdg_wm_dialog_v1_interface, version, wm, wm_bind);
	if (wm->global == NULL) {
		free(wm);
		return NULL;
	}

	wm->display_destroy.notify = xdg_wm_dialog_handle_display_destroy;
	wl_display_add_destroy_listener(display, &wm->display_destroy);

	wl_signal_init(&wm->events.destroy);
	wl_signal_init(&wm->events.new_dialog);

	return wm;
}
