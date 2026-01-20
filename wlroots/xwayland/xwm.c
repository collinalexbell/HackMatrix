#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/util/edges.h>
#include <wlr/util/log.h>
#include <wlr/xcursor.h>
#include <wlr/xwayland/shell.h>
#include <wlr/xwayland/xwayland.h>
#include <xcb/composite.h>
#include <xcb/render.h>
#include <xcb/res.h>
#include <xcb/xfixes.h>
#include "xwayland/xwm.h"

static const char *const atom_map[ATOM_LAST] = {
	[WL_SURFACE_ID] = "WL_SURFACE_ID",
	[WL_SURFACE_SERIAL] = "WL_SURFACE_SERIAL",
	[WM_DELETE_WINDOW] = "WM_DELETE_WINDOW",
	[WM_PROTOCOLS] = "WM_PROTOCOLS",
	[WM_HINTS] = "WM_HINTS",
	[WM_NORMAL_HINTS] = "WM_NORMAL_HINTS",
	[WM_SIZE_HINTS] = "WM_SIZE_HINTS",
	[WM_WINDOW_ROLE] = "WM_WINDOW_ROLE",
	[MOTIF_WM_HINTS] = "_MOTIF_WM_HINTS",
	[UTF8_STRING] = "UTF8_STRING",
	[WM_S0] = "WM_S0",
	[NET_SUPPORTED] = "_NET_SUPPORTED",
	[NET_WM_CM_S0] = "_NET_WM_CM_S0",
	[NET_WM_PID] = "_NET_WM_PID",
	[NET_WM_NAME] = "_NET_WM_NAME",
	[NET_WM_STATE] = "_NET_WM_STATE",
	[NET_WM_STRUT_PARTIAL] = "_NET_WM_STRUT_PARTIAL",
	[NET_WM_WINDOW_TYPE] = "_NET_WM_WINDOW_TYPE",
	[NET_WM_ICON] = "_NET_WM_ICON",
	[WM_TAKE_FOCUS] = "WM_TAKE_FOCUS",
	[WINDOW] = "WINDOW",
	[NET_ACTIVE_WINDOW] = "_NET_ACTIVE_WINDOW",
	[NET_CLOSE_WINDOW] = "_NET_CLOSE_WINDOW",
	[NET_WM_MOVERESIZE] = "_NET_WM_MOVERESIZE",
	[NET_SUPPORTING_WM_CHECK] = "_NET_SUPPORTING_WM_CHECK",
	[NET_WM_STATE_FOCUSED] = "_NET_WM_STATE_FOCUSED",
	[NET_WM_STATE_MODAL] = "_NET_WM_STATE_MODAL",
	[NET_WM_STATE_FULLSCREEN] = "_NET_WM_STATE_FULLSCREEN",
	[NET_WM_STATE_MAXIMIZED_VERT] = "_NET_WM_STATE_MAXIMIZED_VERT",
	[NET_WM_STATE_MAXIMIZED_HORZ] = "_NET_WM_STATE_MAXIMIZED_HORZ",
	[NET_WM_STATE_HIDDEN] = "_NET_WM_STATE_HIDDEN",
	[NET_WM_STATE_STICKY] = "_NET_WM_STATE_STICKY",
	[NET_WM_STATE_SHADED] = "_NET_WM_STATE_SHADED",
	[NET_WM_STATE_SKIP_TASKBAR] = "_NET_WM_STATE_SKIP_TASKBAR",
	[NET_WM_STATE_SKIP_PAGER] = "_NET_WM_STATE_SKIP_PAGER",
	[NET_WM_STATE_ABOVE] = "_NET_WM_STATE_ABOVE",
	[NET_WM_STATE_BELOW] = "_NET_WM_STATE_BELOW",
	[NET_WM_STATE_DEMANDS_ATTENTION] = "_NET_WM_STATE_DEMANDS_ATTENTION",
	[NET_WM_PING] = "_NET_WM_PING",
	[WM_CHANGE_STATE] = "WM_CHANGE_STATE",
	[WM_STATE] = "WM_STATE",
	[CLIPBOARD] = "CLIPBOARD",
	[PRIMARY] = "PRIMARY",
	[WL_SELECTION] = "_WL_SELECTION",
	[TARGETS] = "TARGETS",
	[CLIPBOARD_MANAGER] = "CLIPBOARD_MANAGER",
	[INCR] = "INCR",
	[TEXT] = "TEXT",
	[TIMESTAMP] = "TIMESTAMP",
	[DELETE] = "DELETE",
	[NET_STARTUP_ID] = "_NET_STARTUP_ID",
	[NET_STARTUP_INFO] = "_NET_STARTUP_INFO",
	[NET_STARTUP_INFO_BEGIN] = "_NET_STARTUP_INFO_BEGIN",
	[NET_WM_WINDOW_OPACITY] = "_NET_WM_WINDOW_OPACITY",
	[NET_WM_WINDOW_TYPE_NORMAL] = "_NET_WM_WINDOW_TYPE_NORMAL",
	[NET_WM_WINDOW_TYPE_UTILITY] = "_NET_WM_WINDOW_TYPE_UTILITY",
	[NET_WM_WINDOW_TYPE_TOOLTIP] = "_NET_WM_WINDOW_TYPE_TOOLTIP",
	[NET_WM_WINDOW_TYPE_DND] = "_NET_WM_WINDOW_TYPE_DND",
	[NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = "_NET_WM_WINDOW_TYPE_DROPDOWN_MENU",
	[NET_WM_WINDOW_TYPE_POPUP_MENU] = "_NET_WM_WINDOW_TYPE_POPUP_MENU",
	[NET_WM_WINDOW_TYPE_COMBO] = "_NET_WM_WINDOW_TYPE_COMBO",
	[NET_WM_WINDOW_TYPE_MENU] = "_NET_WM_WINDOW_TYPE_MENU",
	[NET_WM_WINDOW_TYPE_NOTIFICATION] = "_NET_WM_WINDOW_TYPE_NOTIFICATION",
	[NET_WM_WINDOW_TYPE_SPLASH] = "_NET_WM_WINDOW_TYPE_SPLASH",
	[NET_WM_WINDOW_TYPE_DESKTOP] = "_NET_WM_WINDOW_TYPE_DESKTOP",
	[NET_WM_WINDOW_TYPE_DOCK] = "_NET_WM_WINDOW_TYPE_DOCK",
	[NET_WM_WINDOW_TYPE_TOOLBAR] = "_NET_WM_WINDOW_TYPE_TOOLBAR",
	[NET_WM_WINDOW_TYPE_DIALOG] = "_NET_WM_WINDOW_TYPE_DIALOG",
	[DND_SELECTION] = "XdndSelection",
	[DND_AWARE] = "XdndAware",
	[DND_STATUS] = "XdndStatus",
	[DND_POSITION] = "XdndPosition",
	[DND_ENTER] = "XdndEnter",
	[DND_LEAVE] = "XdndLeave",
	[DND_DROP] = "XdndDrop",
	[DND_FINISHED] = "XdndFinished",
	[DND_PROXY] = "XdndProxy",
	[DND_TYPE_LIST] = "XdndTypeList",
	[DND_ACTION_MOVE] = "XdndActionMove",
	[DND_ACTION_COPY] = "XdndActionCopy",
	[DND_ACTION_ASK] = "XdndActionAsk",
	[DND_ACTION_PRIVATE] = "XdndActionPrivate",
	[NET_CLIENT_LIST] = "_NET_CLIENT_LIST",
	[NET_CLIENT_LIST_STACKING] = "_NET_CLIENT_LIST_STACKING",
	[NET_WORKAREA] = "_NET_WORKAREA",
};

#define STARTUP_INFO_REMOVE_PREFIX "remove: ID="
struct pending_startup_id {
	char *msg;
	size_t len;
	xcb_window_t window;
	struct wl_list link;
};

static const struct wlr_addon_interface surface_addon_impl;

struct wlr_xwayland_surface *wlr_xwayland_surface_try_from_wlr_surface(
		struct wlr_surface *surface) {
	struct wlr_addon *addon = wlr_addon_find(&surface->addons, NULL, &surface_addon_impl);
	if (addon == NULL) {
		return NULL;
	}
	struct wlr_xwayland_surface *xsurface = wl_container_of(addon, xsurface, surface_addon);
	return xsurface;
}

// TODO: replace this with hash table?
static struct wlr_xwayland_surface *lookup_surface(struct wlr_xwm *xwm,
		xcb_window_t window_id) {
	struct wlr_xwayland_surface *surface;
	wl_list_for_each(surface, &xwm->surfaces, link) {
		if (surface->window_id == window_id) {
			return surface;
		}
	}
	return NULL;
}

static int xwayland_surface_handle_ping_timeout(void *data) {
	struct wlr_xwayland_surface *surface = data;

	wl_signal_emit_mutable(&surface->events.ping_timeout, NULL);
	surface->pinging = false;
	return 1;
}

static void read_surface_client_id(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_res_query_client_ids_cookie_t cookie) {
	xcb_res_query_client_ids_reply_t *reply = xcb_res_query_client_ids_reply(
		xwm->xcb_conn, cookie, NULL);
	if (reply == NULL) {
		return;
	}

	uint32_t *pid = NULL;
	xcb_res_client_id_value_iterator_t iter =
		xcb_res_query_client_ids_ids_iterator(reply);
	while (iter.rem > 0) {
		if (iter.data->spec.mask & XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID &&
				xcb_res_client_id_value_value_length(iter.data) > 0) {
			pid = xcb_res_client_id_value_value(iter.data);
			break;
		}
		xcb_res_client_id_value_next(&iter);
	}
	if (pid == NULL) {
		free(reply);
		return;
	}
	xsurface->pid = *pid;
	free(reply);
}

static struct wlr_xwayland_surface *xwayland_surface_create(
		struct wlr_xwm *xwm, xcb_window_t window_id, int16_t x, int16_t y,
		uint16_t width, uint16_t height, bool override_redirect) {
	struct wlr_xwayland_surface *surface = calloc(1, sizeof(*surface));
	if (!surface) {
		wlr_log(WLR_ERROR, "Could not allocate wlr xwayland surface");
		return NULL;
	}

	xcb_get_geometry_cookie_t geometry_cookie =
		xcb_get_geometry(xwm->xcb_conn, window_id);

	xcb_res_query_client_ids_cookie_t client_id_cookie = { 0 };
	if (xwm->xres) {
		xcb_res_client_id_spec_t spec = {
			.client = window_id,
			.mask = XCB_RES_CLIENT_ID_MASK_LOCAL_CLIENT_PID
		};
		client_id_cookie = xcb_res_query_client_ids(xwm->xcb_conn, 1, &spec);
	}

	uint32_t values[1];
	values[0] =
		XCB_EVENT_MASK_FOCUS_CHANGE |
		XCB_EVENT_MASK_PROPERTY_CHANGE;
	xcb_change_window_attributes(xwm->xcb_conn, window_id,
		XCB_CW_EVENT_MASK, values);

	surface->xwm = xwm;
	surface->window_id = window_id;
	surface->x = x;
	surface->y = y;
	surface->width = width;
	surface->height = height;
	surface->override_redirect = override_redirect;
	surface->opacity = 1.0;
	wl_list_init(&surface->children);
	wl_list_init(&surface->stack_link);
	wl_list_init(&surface->parent_link);
	wl_list_init(&surface->unpaired_link);

	wl_signal_init(&surface->events.destroy);
	wl_signal_init(&surface->events.request_configure);
	wl_signal_init(&surface->events.request_move);
	wl_signal_init(&surface->events.request_resize);
	wl_signal_init(&surface->events.request_minimize);
	wl_signal_init(&surface->events.request_maximize);
	wl_signal_init(&surface->events.request_fullscreen);
	wl_signal_init(&surface->events.request_activate);
	wl_signal_init(&surface->events.request_close);
	wl_signal_init(&surface->events.request_sticky);
	wl_signal_init(&surface->events.request_shaded);
	wl_signal_init(&surface->events.request_skip_taskbar);
	wl_signal_init(&surface->events.request_skip_pager);
	wl_signal_init(&surface->events.request_above);
	wl_signal_init(&surface->events.request_below);
	wl_signal_init(&surface->events.request_demands_attention);
	wl_signal_init(&surface->events.associate);
	wl_signal_init(&surface->events.dissociate);
	wl_signal_init(&surface->events.set_class);
	wl_signal_init(&surface->events.set_role);
	wl_signal_init(&surface->events.set_title);
	wl_signal_init(&surface->events.set_parent);
	wl_signal_init(&surface->events.set_startup_id);
	wl_signal_init(&surface->events.set_window_type);
	wl_signal_init(&surface->events.set_hints);
	wl_signal_init(&surface->events.set_decorations);
	wl_signal_init(&surface->events.set_strut_partial);
	wl_signal_init(&surface->events.set_override_redirect);
	wl_signal_init(&surface->events.set_geometry);
	wl_signal_init(&surface->events.set_opacity);
	wl_signal_init(&surface->events.set_icon);
	wl_signal_init(&surface->events.focus_in);
	wl_signal_init(&surface->events.grab_focus);
	wl_signal_init(&surface->events.map_request);
	wl_signal_init(&surface->events.ping_timeout);

	xcb_get_geometry_reply_t *geometry_reply =
		xcb_get_geometry_reply(xwm->xcb_conn, geometry_cookie, NULL);
	if (geometry_reply != NULL) {
		surface->has_alpha = geometry_reply->depth == 32;
	}
	free(geometry_reply);

	struct wl_display *display = xwm->xwayland->wl_display;
	struct wl_event_loop *loop = wl_display_get_event_loop(display);
	surface->ping_timer = wl_event_loop_add_timer(loop,
		xwayland_surface_handle_ping_timeout, surface);
	if (surface->ping_timer == NULL) {
		free(surface);
		wlr_log(WLR_ERROR, "Could not add timer to event loop");
		return NULL;
	}

	wl_list_insert(&xwm->surfaces, &surface->link);

	if (xwm->xres) {
		read_surface_client_id(xwm, surface, client_id_cookie);
	}

	wl_signal_emit_mutable(&xwm->xwayland->events.new_surface, surface);

	return surface;
}

static void xwm_set_net_active_window(struct wlr_xwm *xwm,
		xcb_window_t window) {
	xcb_change_property(xwm->xcb_conn, XCB_PROP_MODE_REPLACE,
			xwm->screen->root, xwm->atoms[NET_ACTIVE_WINDOW],
			xwm->atoms[WINDOW], 32, 1, &window);
}

/*
 * Wrapper for xcb_send_event, which ensures that the event data is 32 byte big.
 */
xcb_void_cookie_t xwm_send_event_with_size(xcb_connection_t *c,
		uint8_t propagate, xcb_window_t destination,
		uint32_t event_mask, const void *event, uint32_t length) {
	if (length == 32) {
		return xcb_send_event(c, propagate, destination, event_mask, event);
	} else if (length < 32) {
		char buf[32];
		memcpy(buf, event, length);
		memset(buf + length, 0, 32 - length);
		return xcb_send_event(c, propagate, destination, event_mask, buf);
	} else {
		assert(false && "Event too long");
	}
}

static void xwm_send_wm_message(struct wlr_xwayland_surface *surface,
		xcb_client_message_data_t *data, uint32_t event_mask) {
	struct wlr_xwm *xwm = surface->xwm;

	xcb_client_message_event_t event = {
		.response_type = XCB_CLIENT_MESSAGE,
		.format = 32,
		.sequence = 0,
		.window = surface->window_id,
		.type = xwm->atoms[WM_PROTOCOLS],
		.data = *data,
	};

	xwm_send_event_with_size(xwm->xcb_conn,
		0, // propagate
		surface->window_id,
		event_mask,
		&event,
		sizeof(event));
	xwm_schedule_flush(xwm);
}

static void xwm_set_net_client_list(struct wlr_xwm *xwm) {
	// FIXME: _NET_CLIENT_LIST is expected to be ordered by map time, but the
	// order of surfaces in `xwm->surfaces` is by creation time. The order of
	// windows _NET_CLIENT_LIST exposed by wlroots is wrong.

	size_t mapped_surfaces = 0;
	struct wlr_xwayland_surface *surface;
	wl_list_for_each(surface, &xwm->surfaces, link) {
		if (surface->surface != NULL && surface->surface->mapped) {
			mapped_surfaces++;
		}
	}

	xcb_window_t *windows = NULL;
	if (mapped_surfaces > 0) {
		windows = malloc(sizeof(*windows) * mapped_surfaces);
		if (!windows) {
			return;
		}

		size_t index = 0;
		wl_list_for_each(surface, &xwm->surfaces, link) {
			if (surface->surface != NULL && surface->surface->mapped) {
				windows[index++] = surface->window_id;
			}
		}
	}

	xcb_change_property(xwm->xcb_conn, XCB_PROP_MODE_REPLACE,
			xwm->screen->root, xwm->atoms[NET_CLIENT_LIST],
			XCB_ATOM_WINDOW, 32, mapped_surfaces, windows);
	free(windows);
}

static void xwm_set_net_client_list_stacking(struct wlr_xwm *xwm) {
	size_t num_surfaces = wl_list_length(&xwm->surfaces_in_stack_order);
	xcb_window_t *windows = malloc(sizeof(xcb_window_t) * num_surfaces);
	if (!windows) {
		return;
	}

	size_t i = 0;
	struct wlr_xwayland_surface *xsurface;
	wl_list_for_each(xsurface, &xwm->surfaces_in_stack_order, stack_link) {
		windows[i++] = xsurface->window_id;
	}

	xcb_change_property(xwm->xcb_conn, XCB_PROP_MODE_REPLACE, xwm->screen->root,
			xwm->atoms[NET_CLIENT_LIST_STACKING], XCB_ATOM_WINDOW, 32, num_surfaces,
			windows);
	free(windows);
}

static void xsurface_set_net_wm_state(struct wlr_xwayland_surface *xsurface);

// Gives input (keyboard) focus to a window.
// Normally followed by xwm_set_focused_window().
static void xwm_focus_window(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface) {

	// We handle cases where focus_surface == xsurface because we
	// want to be able to deny FocusIn events.
	if (!xsurface) {
		// XCB_POINTER_ROOT is described in xcb documentation but isn't
		// actually defined in the headers. It's distinct from XCB_NONE
		// (which disables keyboard input entirely and causes issues
		// with keyboard grabs for e.g. popups).
		xcb_set_input_focus_checked(xwm->xcb_conn,
			XCB_INPUT_FOCUS_POINTER_ROOT,
			1L /*XCB_POINTER_ROOT*/, XCB_CURRENT_TIME);
		return;
	}

	if (xsurface->override_redirect) {
		return;
	}

	xcb_client_message_data_t message_data = { 0 };
	message_data.data32[0] = xwm->atoms[WM_TAKE_FOCUS];
	message_data.data32[1] = XCB_TIME_CURRENT_TIME;

	if (xsurface->hints && !xsurface->hints->input) {
		// if the surface doesn't allow the focus request, we will send him
		// only the take focus event. It will get the focus by itself.
		xwm_send_wm_message(xsurface, &message_data, XCB_EVENT_MASK_NO_EVENT);
	} else {
		xwm_send_wm_message(xsurface, &message_data, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT);

		xcb_void_cookie_t cookie = xcb_set_input_focus(xwm->xcb_conn,
			XCB_INPUT_FOCUS_POINTER_ROOT, xsurface->window_id, XCB_CURRENT_TIME);
		xwm->last_focus_seq = cookie.sequence;
	}
}

// Updates _NET_ACTIVE_WINDOW and _NET_WM_STATE when focus changes.
static void xwm_set_focused_window(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface) {
	struct wlr_xwayland_surface *unfocus_surface = xwm->focus_surface;

	if (xsurface && xsurface->override_redirect) {
		return;
	}

	xwm->focus_surface = xsurface;
	// cancel any pending focus offer
	xwm->offered_focus = xsurface;

	if (xsurface == unfocus_surface) {
		return;
	}

	if (unfocus_surface) {
		xsurface_set_net_wm_state(unfocus_surface);
	}

	if (xsurface) {
		xsurface_set_net_wm_state(xsurface);
		xwm_set_net_active_window(xwm, xsurface->window_id);
	} else {
		xwm_set_net_active_window(xwm, xwm->no_focus_window);
	}
}

void wlr_xwayland_surface_offer_focus(struct wlr_xwayland_surface *xsurface) {
	if (!xsurface || xsurface->override_redirect) {
		return;
	}

	struct wlr_xwm *xwm = xsurface->xwm;
	if (!xwm_atoms_contains(xwm, xsurface->protocols,
			xsurface->protocols_len, WM_TAKE_FOCUS)) {
		return;
	}

	xwm->offered_focus = xsurface;

	xcb_client_message_data_t message_data = { 0 };
	message_data.data32[0] = xwm->atoms[WM_TAKE_FOCUS];
	message_data.data32[1] = XCB_TIME_CURRENT_TIME;
	xwm_send_wm_message(xsurface, &message_data, XCB_EVENT_MASK_NO_EVENT);

	xcb_flush(xwm->xcb_conn);
}

static void xwm_surface_activate(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface) {
	if (xsurface && xsurface->override_redirect) {
		return;
	}

	if (xsurface != xwm->focus_surface && xsurface != xwm->offered_focus) {
		xwm_focus_window(xwm, xsurface);
	}

	xwm_set_focused_window(xwm, xsurface);
	xwm_schedule_flush(xwm);
}

static void xsurface_set_net_wm_state(struct wlr_xwayland_surface *xsurface) {
	struct wlr_xwm *xwm = xsurface->xwm;

	// EWMH says _NET_WM_STATE should be unset if the window is withdrawn
	if (xsurface->withdrawn) {
		xcb_delete_property(xwm->xcb_conn,
			xsurface->window_id,
			xwm->atoms[NET_WM_STATE]);
		return;
	}

	uint32_t property[13];
	size_t i = 0;
	if (xsurface->modal) {
		property[i++] = xwm->atoms[NET_WM_STATE_MODAL];
	}
	if (xsurface->fullscreen) {
		property[i++] = xwm->atoms[NET_WM_STATE_FULLSCREEN];
	}
	if (xsurface->maximized_vert) {
		property[i++] = xwm->atoms[NET_WM_STATE_MAXIMIZED_VERT];
	}
	if (xsurface->maximized_horz) {
		property[i++] = xwm->atoms[NET_WM_STATE_MAXIMIZED_HORZ];
	}
	if (xsurface->minimized) {
		property[i++] = xwm->atoms[NET_WM_STATE_HIDDEN];
	}
	if (xsurface->sticky) {
		property[i++] = xwm->atoms[NET_WM_STATE_STICKY];
	}
	if (xsurface->shaded) {
		property[i++] = xwm->atoms[NET_WM_STATE_SHADED];
	}
	if (xsurface->skip_taskbar) {
		property[i++] = xwm->atoms[NET_WM_STATE_SKIP_TASKBAR];
	}
	if (xsurface->skip_pager) {
		property[i++] = xwm->atoms[NET_WM_STATE_SKIP_PAGER];
	}
	if (xsurface->above) {
		property[i++] = xwm->atoms[NET_WM_STATE_ABOVE];
	}
	if (xsurface->below) {
		property[i++] = xwm->atoms[NET_WM_STATE_BELOW];
	}
	if (xsurface->demands_attention) {
		property[i++] = xwm->atoms[NET_WM_STATE_DEMANDS_ATTENTION];
	}
	if (xsurface == xwm->focus_surface) {
		property[i++] = xwm->atoms[NET_WM_STATE_FOCUSED];
	}
	assert(i <= sizeof(property) / sizeof(property[0]));

	xcb_change_property(xwm->xcb_conn,
		XCB_PROP_MODE_REPLACE,
		xsurface->window_id,
		xwm->atoms[NET_WM_STATE],
		XCB_ATOM_ATOM,
		32, // format
		i, property);
}

static void xwayland_surface_dissociate(struct wlr_xwayland_surface *xsurface) {
	if (xsurface->surface != NULL) {
		wlr_surface_unmap(xsurface->surface);
		wl_signal_emit_mutable(&xsurface->events.dissociate, NULL);

		wl_list_remove(&xsurface->surface_commit.link);
		wl_list_remove(&xsurface->surface_map.link);
		wl_list_remove(&xsurface->surface_unmap.link);
		wlr_addon_finish(&xsurface->surface_addon);
		xsurface->surface = NULL;
	}

	// Make sure we're not on the unpaired surface list or we
	// could be assigned a surface during surface creation that
	// was mapped before this unmap request.
	wl_list_remove(&xsurface->unpaired_link);
	wl_list_init(&xsurface->unpaired_link);
	xsurface->surface_id = 0;
	xsurface->serial = 0;

	wl_list_remove(&xsurface->stack_link);
	wl_list_init(&xsurface->stack_link);
	xwm_set_net_client_list_stacking(xsurface->xwm);
}

static void xwayland_surface_destroy(struct wlr_xwayland_surface *xsurface) {
	xwayland_surface_dissociate(xsurface);

	wl_signal_emit_mutable(&xsurface->events.destroy, NULL);

	assert(wl_list_empty(&xsurface->events.destroy.listener_list));
	assert(wl_list_empty(&xsurface->events.request_configure.listener_list));
	assert(wl_list_empty(&xsurface->events.request_move.listener_list));
	assert(wl_list_empty(&xsurface->events.request_resize.listener_list));
	assert(wl_list_empty(&xsurface->events.request_minimize.listener_list));
	assert(wl_list_empty(&xsurface->events.request_maximize.listener_list));
	assert(wl_list_empty(&xsurface->events.request_fullscreen.listener_list));
	assert(wl_list_empty(&xsurface->events.request_activate.listener_list));
	assert(wl_list_empty(&xsurface->events.request_close.listener_list));
	assert(wl_list_empty(&xsurface->events.request_sticky.listener_list));
	assert(wl_list_empty(&xsurface->events.request_shaded.listener_list));
	assert(wl_list_empty(&xsurface->events.request_skip_taskbar.listener_list));
	assert(wl_list_empty(&xsurface->events.request_skip_pager.listener_list));
	assert(wl_list_empty(&xsurface->events.request_above.listener_list));
	assert(wl_list_empty(&xsurface->events.request_below.listener_list));
	assert(wl_list_empty(&xsurface->events.request_demands_attention.listener_list));
	assert(wl_list_empty(&xsurface->events.associate.listener_list));
	assert(wl_list_empty(&xsurface->events.dissociate.listener_list));
	assert(wl_list_empty(&xsurface->events.set_class.listener_list));
	assert(wl_list_empty(&xsurface->events.set_role.listener_list));
	assert(wl_list_empty(&xsurface->events.set_title.listener_list));
	assert(wl_list_empty(&xsurface->events.set_parent.listener_list));
	assert(wl_list_empty(&xsurface->events.set_startup_id.listener_list));
	assert(wl_list_empty(&xsurface->events.set_window_type.listener_list));
	assert(wl_list_empty(&xsurface->events.set_hints.listener_list));
	assert(wl_list_empty(&xsurface->events.set_decorations.listener_list));
	assert(wl_list_empty(&xsurface->events.set_strut_partial.listener_list));
	assert(wl_list_empty(&xsurface->events.set_override_redirect.listener_list));
	assert(wl_list_empty(&xsurface->events.set_geometry.listener_list));
	assert(wl_list_empty(&xsurface->events.set_opacity.listener_list));
	assert(wl_list_empty(&xsurface->events.set_icon.listener_list));
	assert(wl_list_empty(&xsurface->events.focus_in.listener_list));
	assert(wl_list_empty(&xsurface->events.grab_focus.listener_list));
	assert(wl_list_empty(&xsurface->events.map_request.listener_list));
	assert(wl_list_empty(&xsurface->events.ping_timeout.listener_list));

	if (xsurface == xsurface->xwm->focus_surface) {
		xwm_surface_activate(xsurface->xwm, NULL);
	}
	if (xsurface == xsurface->xwm->offered_focus) {
		xsurface->xwm->offered_focus = NULL;
	}

	wl_list_remove(&xsurface->link);
	wl_list_remove(&xsurface->parent_link);

	struct wlr_xwayland_surface *child, *next;
	wl_list_for_each_safe(child, next, &xsurface->children, parent_link) {
		wl_list_remove(&child->parent_link);
		wl_list_init(&child->parent_link);
		child->parent = NULL;
	}

	wl_list_remove(&xsurface->unpaired_link);

	wl_event_source_remove(xsurface->ping_timer);

	free(xsurface->wm_name);
	free(xsurface->net_wm_name);
	free(xsurface->class);
	free(xsurface->instance);
	free(xsurface->role);
	free(xsurface->window_type);
	free(xsurface->protocols);
	free(xsurface->startup_id);
	free(xsurface->hints);
	free(xsurface->size_hints);
	free(xsurface->strut_partial);
	free(xsurface);
}

static void read_surface_class(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *surface, xcb_get_property_reply_t *reply) {
	if (reply->type != XCB_ATOM_STRING && reply->type != xwm->atoms[UTF8_STRING] &&
			reply->type != XCB_ATOM_NONE) {
		wlr_log(WLR_DEBUG, "Invalid WM_CLASS property type");
		return;
	}

	size_t len = xcb_get_property_value_length(reply);
	char *class = xcb_get_property_value(reply);

	// Unpack two sequentially stored strings: instance, class
	size_t instance_len = strnlen(class, len);
	free(surface->instance);
	if (len > 0 && instance_len < len) {
		surface->instance = strndup(class, instance_len);
		class += instance_len + 1;
	} else {
		surface->instance = NULL;
	}
	free(surface->class);
	if (len > 0) {
		surface->class = strndup(class, len);
	} else {
		surface->class = NULL;
	}

	wl_signal_emit_mutable(&surface->events.set_class, NULL);
}

static void read_surface_startup_id(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	if (reply->type != XCB_ATOM_STRING && reply->type != xwm->atoms[UTF8_STRING] &&
			reply->type != XCB_ATOM_NONE) {
		wlr_log(WLR_DEBUG, "Invalid NET_STARTUP_ID property type");
		return;
	}

	size_t len = xcb_get_property_value_length(reply);
	char *startup_id = xcb_get_property_value(reply);

	free(xsurface->startup_id);
	if (len > 0) {
		xsurface->startup_id = strndup(startup_id, len);
	} else {
		xsurface->startup_id = NULL;
	}

	wlr_log(WLR_DEBUG, "XCB_ATOM_NET_STARTUP_ID: %s",
		xsurface->startup_id ? xsurface->startup_id: "(null)");
	wl_signal_emit_mutable(&xsurface->events.set_startup_id, NULL);
}

static void read_surface_opacity(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	if (reply->type == XCB_ATOM_NONE) {
		xsurface->opacity = 1.0;
		wl_signal_emit_mutable(&xsurface->events.set_opacity, NULL);
		return;
	}

	if (reply->type != XCB_ATOM_CARDINAL || reply->format != 32 ||
			xcb_get_property_value_length(reply) != sizeof(uint32_t)) {
		wlr_log(WLR_DEBUG, "Invalid NET_WINDOW_OPACITY property type");
		return;
	}

	uint32_t *val = xcb_get_property_value(reply);
	xsurface->opacity = (double)*val / UINT32_MAX;
	wl_signal_emit_mutable(&xsurface->events.set_opacity, NULL);
}

static void read_surface_role(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	if (reply->type != XCB_ATOM_STRING && reply->type != xwm->atoms[UTF8_STRING] &&
			reply->type != XCB_ATOM_NONE) {
		wlr_log(WLR_DEBUG, "Invalid WM_WINDOW_ROLE property type");
		return;
	}

	size_t len = xcb_get_property_value_length(reply);
	char *role = xcb_get_property_value(reply);

	free(xsurface->role);
	if (len > 0) {
		xsurface->role = strndup(role, len);
	} else {
		xsurface->role = NULL;
	}

	wl_signal_emit_mutable(&xsurface->events.set_role, NULL);
}

static void read_surface_title(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface, xcb_atom_t property,
		xcb_get_property_reply_t *reply) {
	if (reply->type != XCB_ATOM_STRING && reply->type != xwm->atoms[UTF8_STRING] &&
			reply->type != XCB_ATOM_NONE) {
		wlr_log(WLR_DEBUG, "Invalid WM_NAME/NET_WM_NAME property type");
		return;
	}

	size_t len = xcb_get_property_value_length(reply);
	const char *title_buffer = xcb_get_property_value(reply);
	char *title = NULL;
	if (len > 0) {
		title = strndup(title_buffer, len);
	}

	if (property == XCB_ATOM_WM_NAME) {
		free(xsurface->wm_name);
		xsurface->wm_name = title;
	} else if (property == xwm->atoms[NET_WM_NAME]) {
		free(xsurface->net_wm_name);
		xsurface->net_wm_name = title;
	} else {
		abort(); // unreachable
	}

	// Prefer _NET_WM_NAME over WM_NAME if available
	if (xsurface->net_wm_name != NULL) {
		xsurface->title = xsurface->net_wm_name;
	} else if (xsurface->wm_name != NULL) {
		xsurface->title = xsurface->wm_name;
	} else {
		xsurface->title = NULL;
	}

	wl_signal_emit_mutable(&xsurface->events.set_title, NULL);
}

static bool has_parent(struct wlr_xwayland_surface *parent,
		struct wlr_xwayland_surface *child) {
	while (parent) {
		if (child == parent) {
			return true;
		}

		parent = parent->parent;
	}

	return false;
}

static void read_surface_parent(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	if (reply->type != XCB_ATOM_WINDOW && reply->type != XCB_ATOM_NONE) {
		wlr_log(WLR_DEBUG, "Invalid WM_TRANSIENT_FOR property type");
		return;
	}

	struct wlr_xwayland_surface *found_parent = NULL;
	xcb_window_t *xid = xcb_get_property_value(reply);
	if (reply->type != XCB_ATOM_NONE && xid != NULL) {
		found_parent = lookup_surface(xwm, *xid);
		if (!has_parent(found_parent, xsurface)) {
			xsurface->parent = found_parent;
		} else {
			wlr_log(WLR_INFO, "%p with %p would create a loop", xsurface,
						found_parent);
		}
	} else {
		xsurface->parent = NULL;
	}

	wl_list_remove(&xsurface->parent_link);
	if (xsurface->parent != NULL) {
		wl_list_insert(&xsurface->parent->children, &xsurface->parent_link);
	} else {
		wl_list_init(&xsurface->parent_link);
	}

	wl_signal_emit_mutable(&xsurface->events.set_parent, NULL);
}

static void read_surface_window_type(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	if (reply->type != XCB_ATOM_ATOM && reply->type != XCB_ATOM_NONE) {
		wlr_log(WLR_DEBUG, "Invalid NET_WM_WINDOW_TYPE property type");
		return;
	}

	xcb_atom_t *atoms = xcb_get_property_value(reply);
	size_t atoms_len = reply->value_len;
	size_t atoms_size = sizeof(xcb_atom_t) * atoms_len;

	free(xsurface->window_type);
	if (atoms_len > 0) {
		xsurface->window_type = malloc(atoms_size);
		if (xsurface->window_type == NULL) {
			return;
		}
		memcpy(xsurface->window_type, atoms, atoms_size);
	} else {
		xsurface->window_type = NULL;
	}
	xsurface->window_type_len = atoms_len;

	wl_signal_emit_mutable(&xsurface->events.set_window_type, NULL);
}

static void read_surface_protocols(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	if (reply->type != XCB_ATOM_ATOM && reply->type != XCB_ATOM_NONE) {
		wlr_log(WLR_DEBUG, "Invalid WM_PROTOCOLS property type");
		return;
	}

	xcb_atom_t *atoms = xcb_get_property_value(reply);
	size_t atoms_len = reply->value_len;
	size_t atoms_size = sizeof(xcb_atom_t) * atoms_len;

	free(xsurface->protocols);
	if (atoms_len > 0) {
		xsurface->protocols = malloc(atoms_size);
		if (xsurface->protocols == NULL) {
			return;
		}
		memcpy(xsurface->protocols, atoms, atoms_size);
	} else {
		xsurface->protocols = NULL;
	}
	xsurface->protocols_len = atoms_len;
}

static void read_surface_hints(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	// According to the docs, reply->type == xwm->atoms[WM_HINTS]
	// In practice, reply->type == XCB_ATOM_ATOM
	if (reply->type != xwm->atoms[WM_HINTS] && reply->type != XCB_ATOM_ATOM &&
			reply->type != XCB_ATOM_NONE) {
		wlr_log(WLR_DEBUG, "Invalid WM_HINTS property type");
		return;
	}

	free(xsurface->hints);
	if (reply->value_len > 0) {
		xsurface->hints = calloc(1, sizeof(*xsurface->hints));
		if (xsurface->hints == NULL) {
			return;
		}
		xcb_icccm_get_wm_hints_from_reply(xsurface->hints, reply);

		if (!(xsurface->hints->flags & XCB_ICCCM_WM_HINT_INPUT)) {
			// The client didn't specify whether it wants input.
			// Assume it does.
			xsurface->hints->input = true;
		}
	} else {
		xsurface->hints = NULL;
	}

	wl_signal_emit_mutable(&xsurface->events.set_hints, NULL);
}

static void read_surface_normal_hints(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	if (reply->type != xwm->atoms[WM_SIZE_HINTS] && reply->type != XCB_ATOM_NONE) {
		wlr_log(WLR_DEBUG, "Invalid WM_NORMAL_HINTS property type");
		return;
	}

	free(xsurface->size_hints);
	xsurface->size_hints = NULL;

	if (reply->value_len == 0) {
		return;
	}

	xsurface->size_hints = calloc(1, sizeof(*xsurface->size_hints));
	if (xsurface->size_hints == NULL) {
		return;
	}
	xcb_icccm_get_wm_size_hints_from_reply(xsurface->size_hints, reply);

	int32_t flags = xsurface->size_hints->flags;
	bool has_min_size_hints = (flags & XCB_ICCCM_SIZE_HINT_P_MIN_SIZE) != 0;
	bool has_base_size_hints = (flags & XCB_ICCCM_SIZE_HINT_BASE_SIZE) != 0;
	/* ICCCM says that if absent, min size is equal to base size and vice versa */
	if (!has_min_size_hints && !has_base_size_hints) {
		xsurface->size_hints->min_width = -1;
		xsurface->size_hints->min_height = -1;
		xsurface->size_hints->base_width = -1;
		xsurface->size_hints->base_height = -1;
	} else if (!has_base_size_hints) {
		xsurface->size_hints->base_width = xsurface->size_hints->min_width;
		xsurface->size_hints->base_height = xsurface->size_hints->min_height;
	} else if (!has_min_size_hints) {
		xsurface->size_hints->min_width = xsurface->size_hints->base_width;
		xsurface->size_hints->min_height = xsurface->size_hints->base_height;
	}

	if ((flags & XCB_ICCCM_SIZE_HINT_P_MAX_SIZE) == 0) {
		xsurface->size_hints->max_width = -1;
		xsurface->size_hints->max_height = -1;
	}
}

#define MWM_HINTS_FLAGS_FIELD 0
#define MWM_HINTS_DECORATIONS_FIELD 2

#define MWM_HINTS_DECORATIONS (1 << 1)

#define MWM_DECOR_ALL (1 << 0)
#define MWM_DECOR_BORDER (1 << 1)
#define MWM_DECOR_TITLE (1 << 3)

static void read_surface_motif_hints(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	if (reply->value_len == 0) {
		xsurface->decorations = 0;
		wl_signal_emit_mutable(&xsurface->events.set_decorations, NULL);
		return;
	}

	if (reply->value_len < 5) {
		wlr_log(WLR_DEBUG, "Invalid MOTIF_WM_HINTS property type");
		return;
	}

	uint32_t *motif_hints = xcb_get_property_value(reply);
	if (motif_hints[MWM_HINTS_FLAGS_FIELD] & MWM_HINTS_DECORATIONS) {
		xsurface->decorations = WLR_XWAYLAND_SURFACE_DECORATIONS_ALL;
		uint32_t decorations = motif_hints[MWM_HINTS_DECORATIONS_FIELD];
		if ((decorations & MWM_DECOR_ALL) == 0) {
			if ((decorations & MWM_DECOR_BORDER) == 0) {
				xsurface->decorations |=
					WLR_XWAYLAND_SURFACE_DECORATIONS_NO_BORDER;
			}
			if ((decorations & MWM_DECOR_TITLE) == 0) {
				xsurface->decorations |=
					WLR_XWAYLAND_SURFACE_DECORATIONS_NO_TITLE;
			}
		}
		wl_signal_emit_mutable(&xsurface->events.set_decorations, NULL);
	}
}

static void read_surface_strut_partial(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	free(xsurface->strut_partial);
	xsurface->strut_partial = NULL;

	if (reply->type == XCB_ATOM_NONE) {
		wl_signal_emit_mutable(&xsurface->events.set_strut_partial, NULL);
		return;
	}

	if (reply->type != XCB_ATOM_CARDINAL || reply->format != 32 ||
			xcb_get_property_value_length(reply) !=
			sizeof(xcb_ewmh_wm_strut_partial_t)) {
		wlr_log(WLR_DEBUG, "Invalid NET_WM_STRUT_PARTIAL property type");
		return;
	}

	xsurface->strut_partial = calloc(1, sizeof(*xsurface->strut_partial));
	if (xsurface->strut_partial == NULL) {
		return;
	}
	xcb_ewmh_get_wm_strut_partial_from_reply(xsurface->strut_partial, reply);
	wl_signal_emit_mutable(&xsurface->events.set_strut_partial, NULL);
}

static void read_surface_net_wm_state(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface,
		xcb_get_property_reply_t *reply) {
	xsurface->fullscreen = 0;
	xcb_atom_t *atom = xcb_get_property_value(reply);
	for (uint32_t i = 0; i < reply->value_len; i++) {
		if (atom[i] == xwm->atoms[NET_WM_STATE_MODAL]) {
			xsurface->modal = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_FULLSCREEN]) {
			xsurface->fullscreen = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_MAXIMIZED_VERT]) {
			xsurface->maximized_vert = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_MAXIMIZED_HORZ]) {
			xsurface->maximized_horz = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_HIDDEN]) {
			xsurface->minimized = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_STICKY]) {
			xsurface->sticky = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_SHADED]) {
			xsurface->shaded = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_SKIP_TASKBAR]) {
			xsurface->skip_taskbar = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_SKIP_PAGER]) {
			xsurface->skip_pager = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_ABOVE]) {
			xsurface->above = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_BELOW]) {
			xsurface->below = true;
		} else if (atom[i] == xwm->atoms[NET_WM_STATE_DEMANDS_ATTENTION]) {
			xsurface->demands_attention = true;
		}
	}
}

char *xwm_get_atom_name(struct wlr_xwm *xwm, xcb_atom_t atom) {
	xcb_get_atom_name_cookie_t name_cookie =
		xcb_get_atom_name(xwm->xcb_conn, atom);
	xcb_get_atom_name_reply_t *name_reply =
		xcb_get_atom_name_reply(xwm->xcb_conn, name_cookie, NULL);
	if (name_reply == NULL) {
		return NULL;
	}
	size_t len = xcb_get_atom_name_name_length(name_reply);
	char *buf = xcb_get_atom_name_name(name_reply); // not a C string
	char *name = strndup(buf, len);
	free(name_reply);
	return name;
}

static void read_surface_property(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface, xcb_atom_t property,
		xcb_get_property_reply_t *reply) {
	if (property == XCB_ATOM_WM_CLASS) {
		read_surface_class(xwm, xsurface, reply);
	} else if (property == XCB_ATOM_WM_NAME ||
			property == xwm->atoms[NET_WM_NAME]) {
		read_surface_title(xwm, xsurface, property, reply);
	} else if (property == XCB_ATOM_WM_TRANSIENT_FOR) {
		read_surface_parent(xwm, xsurface, reply);
	} else if (property == xwm->atoms[NET_WM_PID]) {
		// intentionally ignored
	} else if (property == xwm->atoms[NET_WM_WINDOW_TYPE]) {
		read_surface_window_type(xwm, xsurface, reply);
	} else if (property == xwm->atoms[NET_WM_ICON]) {
		wl_signal_emit_mutable(&xsurface->events.set_icon, NULL);
	} else if (property == xwm->atoms[WM_PROTOCOLS]) {
		read_surface_protocols(xwm, xsurface, reply);
	} else if (property == xwm->atoms[NET_WM_STATE]) {
		read_surface_net_wm_state(xwm, xsurface, reply);
	} else if (property == xwm->atoms[WM_HINTS]) {
		read_surface_hints(xwm, xsurface, reply);
	} else if (property == xwm->atoms[WM_NORMAL_HINTS]) {
		read_surface_normal_hints(xwm, xsurface, reply);
	} else if (property == xwm->atoms[MOTIF_WM_HINTS]) {
		read_surface_motif_hints(xwm, xsurface, reply);
	} else if (property == xwm->atoms[NET_WM_STRUT_PARTIAL]) {
		read_surface_strut_partial(xwm, xsurface, reply);
	} else if (property == xwm->atoms[WM_WINDOW_ROLE]) {
		read_surface_role(xwm, xsurface, reply);
	} else if (property == xwm->atoms[NET_STARTUP_ID]) {
		read_surface_startup_id(xwm, xsurface, reply);
	} else if (property == xwm->atoms[NET_WM_WINDOW_OPACITY]) {
		read_surface_opacity(xwm, xsurface, reply);
	} else if (wlr_log_get_verbosity() >= WLR_DEBUG) {
		char *prop_name = xwm_get_atom_name(xwm, property);
		wlr_log(WLR_DEBUG, "unhandled X11 property %" PRIu32 " (%s) for window %" PRIu32,
			property, prop_name ? prop_name : "(null)", xsurface->window_id);
		free(prop_name);
	}
}

static void xwayland_surface_handle_commit(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = wl_container_of(listener, xsurface, surface_commit);
	if (wlr_surface_has_buffer(xsurface->surface)) {
		wlr_surface_map(xsurface->surface);
	}
}

static void xwayland_surface_handle_map(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = wl_container_of(listener, xsurface, surface_map);
	xwm_set_net_client_list(xsurface->xwm);
}

static void xwayland_surface_handle_unmap(struct wl_listener *listener, void *data) {
	struct wlr_xwayland_surface *xsurface = wl_container_of(listener, xsurface, surface_unmap);
	xwm_set_net_client_list(xsurface->xwm);
}

static void xwayland_surface_handle_addon_destroy(struct wlr_addon *addon) {
	struct wlr_xwayland_surface *xsurface = wl_container_of(addon, xsurface, surface_addon);
	xwayland_surface_dissociate(xsurface);
}

static const struct wlr_addon_interface surface_addon_impl = {
	.name = "wlr_xwayland_surface",
	.destroy = xwayland_surface_handle_addon_destroy,
};

bool wlr_xwayland_surface_fetch_icon(
		const struct wlr_xwayland_surface *xsurface,
		xcb_ewmh_get_wm_icon_reply_t *icon_reply) {
	struct wlr_xwm *xwm = xsurface->xwm;

	xcb_get_property_cookie_t cookie = xcb_get_property(xwm->xcb_conn, 0,
		xsurface->window_id, xwm->atoms[NET_WM_ICON], XCB_ATOM_CARDINAL,
		0, UINT32_MAX);
	xcb_get_property_reply_t *reply =
		xcb_get_property_reply(xwm->xcb_conn, cookie, NULL);
	if (!reply) {
		return false;
	}

	if (!xcb_ewmh_get_wm_icon_from_reply(icon_reply, reply)) {
		free(reply);
		return false;
	}

	return true;
}

static xcb_get_property_cookie_t get_property(struct wlr_xwm *xwm,
		xcb_window_t window_id, xcb_atom_t atom) {
	uint32_t len = 2048;
	if (atom == xwm->atoms[NET_WM_ICON]) {
		/* Compositors need to fetch icon data wlr_xwayland_surface_fetch_icon() */
		len = 0;
	}
	return xcb_get_property(xwm->xcb_conn, 0, window_id, atom, XCB_ATOM_ANY, 0, len);
}

static void xwayland_surface_associate(struct wlr_xwm *xwm,
		struct wlr_xwayland_surface *xsurface, struct wlr_surface *surface) {
	assert(xsurface->surface == NULL);

	wl_list_remove(&xsurface->unpaired_link);
	wl_list_init(&xsurface->unpaired_link);
	xsurface->surface_id = 0;

	xsurface->surface = surface;
	wlr_addon_init(&xsurface->surface_addon, &surface->addons, NULL, &surface_addon_impl);

	xsurface->surface_commit.notify = xwayland_surface_handle_commit;
	wl_signal_add(&surface->events.commit, &xsurface->surface_commit);

	xsurface->surface_map.notify = xwayland_surface_handle_map;
	wl_signal_add(&surface->events.map, &xsurface->surface_map);

	xsurface->surface_unmap.notify = xwayland_surface_handle_unmap;
	wl_signal_add(&surface->events.unmap, &xsurface->surface_unmap);

	// read all surface properties
	const xcb_atom_t props[] = {
		XCB_ATOM_WM_CLASS,
		XCB_ATOM_WM_NAME,
		XCB_ATOM_WM_TRANSIENT_FOR,
		xwm->atoms[WM_PROTOCOLS],
		xwm->atoms[WM_HINTS],
		xwm->atoms[WM_NORMAL_HINTS],
		xwm->atoms[MOTIF_WM_HINTS],
		xwm->atoms[NET_STARTUP_ID],
		xwm->atoms[NET_WM_STATE],
		xwm->atoms[NET_WM_STRUT_PARTIAL],
		xwm->atoms[NET_WM_WINDOW_TYPE],
		xwm->atoms[NET_WM_NAME],
		xwm->atoms[NET_WM_ICON],
	};

	xcb_get_property_cookie_t cookies[sizeof(props) / sizeof(props[0])] = {0};
	for (size_t i = 0; i < sizeof(props) / sizeof(props[0]); i++) {
		cookies[i] = get_property(xwm, xsurface->window_id, props[i]);
	}

	for (size_t i = 0; i < sizeof(props) / sizeof(props[0]); i++) {
		xcb_get_property_reply_t *reply =
			xcb_get_property_reply(xwm->xcb_conn, cookies[i], NULL);
		if (reply == NULL) {
			wlr_log(WLR_ERROR, "Failed to get window property");
			continue;
		}
		read_surface_property(xwm, xsurface, props[i], reply);
		free(reply);
	}

	wl_signal_emit_mutable(&xsurface->events.associate, NULL);
}

static void xwm_handle_create_notify(struct wlr_xwm *xwm,
		xcb_create_notify_event_t *ev) {
	if (ev->window == xwm->window ||
			ev->window == xwm->primary_selection.window ||
			ev->window == xwm->clipboard_selection.window ||
			ev->window == xwm->dnd_selection.window) {
		return;
	}

	xwayland_surface_create(xwm, ev->window, ev->x, ev->y,
		ev->width, ev->height, ev->override_redirect);
}

static void xwm_handle_destroy_notify(struct wlr_xwm *xwm,
		xcb_destroy_notify_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	if (xsurface == NULL) {
		return;
	}
	xwayland_surface_destroy(xsurface);
	xwm_handle_selection_destroy_notify(xwm, ev);
}

static void xwm_handle_configure_request(struct wlr_xwm *xwm,
		xcb_configure_request_event_t *ev) {
	struct wlr_xwayland_surface *surface = lookup_surface(xwm, ev->window);
	if (surface == NULL) {
		return;
	}

	// TODO: handle ev->{parent,sibling}?

	uint16_t mask = ev->value_mask;
	uint16_t geo_mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
		XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT;
	if ((mask & geo_mask) == 0) {
		return;
	}

	struct wlr_xwayland_surface_configure_event wlr_event = {
		.surface = surface,
		.x = mask & XCB_CONFIG_WINDOW_X ? ev->x : surface->x,
		.y = mask & XCB_CONFIG_WINDOW_Y ? ev->y : surface->y,
		.width = mask & XCB_CONFIG_WINDOW_WIDTH ? ev->width : surface->width,
		.height = mask & XCB_CONFIG_WINDOW_HEIGHT ? ev->height : surface->height,
		.mask = mask,
	};

	wl_signal_emit_mutable(&surface->events.request_configure, &wlr_event);
}

static void xwm_update_override_redirect(struct wlr_xwayland_surface *xsurface,
		bool override_redirect) {
	if (xsurface->override_redirect == override_redirect) {
		return;
	}
	xsurface->override_redirect = override_redirect;

	if (override_redirect) {
		wl_list_remove(&xsurface->stack_link);
		wl_list_init(&xsurface->stack_link);
		xwm_set_net_client_list_stacking(xsurface->xwm);
	} else if (xsurface->surface != NULL && xsurface->surface->mapped) {
		wlr_xwayland_surface_restack(xsurface, NULL, XCB_STACK_MODE_BELOW);
	}

	wl_signal_emit_mutable(&xsurface->events.set_override_redirect, NULL);
}

static void xwm_handle_configure_notify(struct wlr_xwm *xwm,
		xcb_configure_notify_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	if (!xsurface) {
		return;
	}

	bool geometry_changed =
		(xsurface->x != ev->x || xsurface->y != ev->y ||
		 xsurface->width != ev->width || xsurface->height != ev->height);

	if (geometry_changed) {
		xsurface->x = ev->x;
		xsurface->y = ev->y;
		xsurface->width = ev->width;
		xsurface->height = ev->height;
	}

	xwm_update_override_redirect(xsurface, ev->override_redirect);

	if (geometry_changed) {
		wl_signal_emit_mutable(&xsurface->events.set_geometry, NULL);
	}
}

static void xsurface_set_wm_state(struct wlr_xwayland_surface *xsurface) {
	struct wlr_xwm *xwm = xsurface->xwm;
	uint32_t property[] = { XCB_ICCCM_WM_STATE_NORMAL, XCB_WINDOW_NONE };

	if (xsurface->withdrawn) {
		property[0] = XCB_ICCCM_WM_STATE_WITHDRAWN;
	} else if (xsurface->minimized) {
		property[0] = XCB_ICCCM_WM_STATE_ICONIC;
	} else {
		property[0] = XCB_ICCCM_WM_STATE_NORMAL;
	}

	xcb_change_property(xwm->xcb_conn,
		XCB_PROP_MODE_REPLACE,
		xsurface->window_id,
		xwm->atoms[WM_STATE],
		xwm->atoms[WM_STATE],
		32, // format
		sizeof(property) / sizeof(property[0]), property);
}

void wlr_xwayland_surface_restack(struct wlr_xwayland_surface *xsurface,
		struct wlr_xwayland_surface *sibling, enum xcb_stack_mode_t mode) {
	struct wlr_xwm *xwm = xsurface->xwm;
	uint32_t values[2];
	size_t idx = 0;
	uint32_t flags = XCB_CONFIG_WINDOW_STACK_MODE;

	assert(!xsurface->override_redirect);

	// X11 clients expect their override_redirect windows to stay on top.
	// Avoid interfering by restacking above the topmost managed surface.
	if (mode == XCB_STACK_MODE_ABOVE && !sibling) {
		sibling = wl_container_of(xwm->surfaces_in_stack_order.prev, sibling, stack_link);
	}

	if (sibling == xsurface) {
		return;
	}

	if (sibling != NULL) {
		values[idx++] = sibling->window_id;
		flags |= XCB_CONFIG_WINDOW_SIBLING;
	}
	values[idx++] = mode;

	xcb_configure_window(xwm->xcb_conn, xsurface->window_id, flags, values);

	wl_list_remove(&xsurface->stack_link);

	struct wl_list *node;
	if (mode == XCB_STACK_MODE_ABOVE) {
		node = &sibling->stack_link;
	} else if (mode == XCB_STACK_MODE_BELOW) {
		if (sibling) {
			node = sibling->stack_link.prev;
		} else {
			node = &xwm->surfaces_in_stack_order;
		}
	} else {
		// Not implementing XCB_STACK_MODE_TOP_IF | XCB_STACK_MODE_BOTTOM_IF |
		// XCB_STACK_MODE_OPPOSITE.
		abort();
	}

	wl_list_insert(node, &xsurface->stack_link);
	xwm_set_net_client_list_stacking(xwm);
	xwm_schedule_flush(xwm);
}

static void xwm_handle_map_request(struct wlr_xwm *xwm,
		xcb_map_request_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	if (!xsurface) {
		return;
	}

	wl_signal_emit_mutable(&xsurface->events.map_request, NULL);
	xcb_map_window(xwm->xcb_conn, ev->window);
}

static void xwm_handle_map_notify(struct wlr_xwm *xwm,
		xcb_map_notify_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	if (!xsurface) {
		return;
	}

	xwm_update_override_redirect(xsurface, ev->override_redirect);

	if (!xsurface->override_redirect) {
		wlr_xwayland_surface_set_withdrawn(xsurface, false);
		wlr_xwayland_surface_restack(xsurface, NULL, XCB_STACK_MODE_BELOW);
	}
}

static void xwm_handle_unmap_notify(struct wlr_xwm *xwm,
		xcb_unmap_notify_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	if (xsurface == NULL) {
		return;
	}

	xwayland_surface_dissociate(xsurface);

	if (!xsurface->override_redirect) {
		wlr_xwayland_surface_set_withdrawn(xsurface, true);
	}
}

static void xwm_handle_property_notify(struct wlr_xwm *xwm,
		xcb_property_notify_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	if (xsurface == NULL) {
		return;
	}

	xcb_get_property_cookie_t cookie = get_property(xwm, xsurface->window_id, ev->atom);
	xcb_get_property_reply_t *reply =
		xcb_get_property_reply(xwm->xcb_conn, cookie, NULL);
	if (reply == NULL) {
		wlr_log(WLR_ERROR, "Failed to get window property");
		return;
	}

	read_surface_property(xwm, xsurface, ev->atom, reply);
	free(reply);
}

static void xwm_handle_surface_id_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	if (xsurface == NULL) {
		wlr_log(WLR_DEBUG,
			"client message WL_SURFACE_ID but no new window %u ?",
			ev->window);
		return;
	}
	/* Check if we got notified after wayland surface create event */
	uint32_t id = ev->data.data32[0];
	struct wl_resource *resource =
		wl_client_get_object(xwm->xwayland->server->client, id);
	if (resource) {
		struct wlr_surface *surface = wlr_surface_from_resource(resource);
		xwayland_surface_associate(xwm, xsurface, surface);
	} else {
		xsurface->surface_id = id;
		wl_list_remove(&xsurface->unpaired_link);
		wl_list_insert(&xwm->unpaired_surfaces, &xsurface->unpaired_link);
	}
}

static void xwm_handle_surface_serial_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	if (xsurface == NULL) {
		wlr_log(WLR_DEBUG,
			"Received client message WL_SURFACE_SERIAL but no X11 window %u",
			ev->window);
		return;
	}
	if (xsurface->serial != 0) {
		wlr_log(WLR_DEBUG, "Received multiple client messages WL_SURFACE_SERIAL "
			"for the same X11 window %u", ev->window);
		return;
	}

	uint32_t serial_lo = ev->data.data32[0];
	uint32_t serial_hi = ev->data.data32[1];
	xsurface->serial = ((uint64_t)serial_hi << 32) | serial_lo;

	struct wlr_surface *surface = wlr_xwayland_shell_v1_surface_from_serial(
		xwm->xwayland->shell_v1, xsurface->serial);
	if (surface != NULL) {
		xwayland_surface_associate(xwm, xsurface, surface);
	} else {
		wl_list_remove(&xsurface->unpaired_link);
		wl_list_insert(&xwm->unpaired_surfaces, &xsurface->unpaired_link);
	}
}

#define _NET_WM_MOVERESIZE_SIZE_TOPLEFT 0
#define _NET_WM_MOVERESIZE_SIZE_TOP 1
#define _NET_WM_MOVERESIZE_SIZE_TOPRIGHT 2
#define _NET_WM_MOVERESIZE_SIZE_RIGHT 3
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT 4
#define _NET_WM_MOVERESIZE_SIZE_BOTTOM 5
#define _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT 6
#define _NET_WM_MOVERESIZE_SIZE_LEFT 7
#define _NET_WM_MOVERESIZE_MOVE 8  // movement only
#define _NET_WM_MOVERESIZE_SIZE_KEYBOARD 9  // size via keyboard
#define _NET_WM_MOVERESIZE_MOVE_KEYBOARD 10  // move via keyboard
#define _NET_WM_MOVERESIZE_CANCEL 11  // cancel operation

static enum wlr_edges net_wm_edges_to_wlr(uint32_t net_wm_edges) {
	switch(net_wm_edges) {
	case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
		return WLR_EDGE_TOP | WLR_EDGE_LEFT;
	case _NET_WM_MOVERESIZE_SIZE_TOP:
		return WLR_EDGE_TOP;
	case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
		return WLR_EDGE_TOP | WLR_EDGE_RIGHT;
	case _NET_WM_MOVERESIZE_SIZE_RIGHT:
		return WLR_EDGE_RIGHT;
	case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
		return WLR_EDGE_BOTTOM | WLR_EDGE_RIGHT;
	case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
		return WLR_EDGE_BOTTOM;
	case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
		return WLR_EDGE_BOTTOM | WLR_EDGE_LEFT;
	case _NET_WM_MOVERESIZE_SIZE_LEFT:
		return WLR_EDGE_LEFT;
	default:
		return WLR_EDGE_NONE;
	}
}

static void xwm_handle_net_wm_moveresize_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	if (!xsurface) {
		return;
	}

	int detail = ev->data.data32[2];
	switch (detail) {
	case _NET_WM_MOVERESIZE_MOVE:
		wl_signal_emit_mutable(&xsurface->events.request_move, NULL);
		break;
	case _NET_WM_MOVERESIZE_SIZE_TOPLEFT:
	case _NET_WM_MOVERESIZE_SIZE_TOP:
	case _NET_WM_MOVERESIZE_SIZE_TOPRIGHT:
	case _NET_WM_MOVERESIZE_SIZE_RIGHT:
	case _NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT:
	case _NET_WM_MOVERESIZE_SIZE_BOTTOM:
	case _NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT:
	case _NET_WM_MOVERESIZE_SIZE_LEFT:;
		struct wlr_xwayland_resize_event resize_event = {
			.surface = xsurface,
			.edges = net_wm_edges_to_wlr(detail),
		};
		wl_signal_emit_mutable(&xsurface->events.request_resize, &resize_event);
		break;
	case _NET_WM_MOVERESIZE_CANCEL:
		// handled by the compositor
		break;
	}
}

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2

static bool update_state(int action, bool *state) {
	int new_state, changed;

	switch (action) {
	case _NET_WM_STATE_REMOVE:
		new_state = false;
		break;
	case _NET_WM_STATE_ADD:
		new_state = true;
		break;
	case _NET_WM_STATE_TOGGLE:
		new_state = !*state;
		break;
	default:
		return false;
	}

	changed = (*state != new_state);
	*state = new_state;

	return changed;
}

static void xwm_handle_net_wm_state_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *client_message) {
	struct wlr_xwayland_surface *xsurface =
		lookup_surface(xwm, client_message->window);
	if (!xsurface) {
		return;
	}
	if (client_message->format != 32) {
		return;
	}

	bool fullscreen = xsurface->fullscreen;
	bool maximized_vert = xsurface->maximized_vert;
	bool maximized_horz = xsurface->maximized_horz;
	bool minimized = xsurface->minimized;
	bool sticky = xsurface->sticky;
	bool shaded = xsurface->shaded;
	bool skip_taskbar = xsurface->skip_taskbar;
	bool skip_pager = xsurface->skip_pager;
	bool above = xsurface->above;
	bool below = xsurface->below;
	bool demands_attention = xsurface->demands_attention;

	uint32_t action = client_message->data.data32[0];
	for (size_t i = 0; i < 2; ++i) {
		xcb_atom_t property = client_message->data.data32[1 + i];

		bool changed = false;
		if (property == xwm->atoms[NET_WM_STATE_MODAL]) {
			changed = update_state(action, &xsurface->modal);
		} else if (property == xwm->atoms[NET_WM_STATE_FULLSCREEN]) {
			changed = update_state(action, &xsurface->fullscreen);
		} else if (property == xwm->atoms[NET_WM_STATE_MAXIMIZED_VERT]) {
			changed = update_state(action, &xsurface->maximized_vert);
		} else if (property == xwm->atoms[NET_WM_STATE_MAXIMIZED_HORZ]) {
			changed = update_state(action, &xsurface->maximized_horz);
		} else if (property == xwm->atoms[NET_WM_STATE_HIDDEN]) {
			changed = update_state(action, &xsurface->minimized);
		} else if (property == xwm->atoms[NET_WM_STATE_STICKY]) {
			changed = update_state(action, &xsurface->sticky);
		} else if (property == xwm->atoms[NET_WM_STATE_SHADED]) {
			changed = update_state(action, &xsurface->shaded);
		} else if (property == xwm->atoms[NET_WM_STATE_SKIP_TASKBAR]) {
			changed = update_state(action, &xsurface->skip_taskbar);
		} else if (property == xwm->atoms[NET_WM_STATE_SKIP_PAGER]) {
			changed = update_state(action, &xsurface->skip_pager);
		} else if (property == xwm->atoms[NET_WM_STATE_ABOVE]) {
			changed = update_state(action, &xsurface->above);
		} else if (property == xwm->atoms[NET_WM_STATE_BELOW]) {
			changed = update_state(action, &xsurface->below);
		} else if (property == xwm->atoms[NET_WM_STATE_DEMANDS_ATTENTION]) {
			changed = update_state(action, &xsurface->demands_attention);
		} else if (property != XCB_ATOM_NONE && wlr_log_get_verbosity() >= WLR_DEBUG) {
			char *prop_name = xwm_get_atom_name(xwm, property);
			wlr_log(WLR_DEBUG, "Unhandled NET_WM_STATE property change "
				"%"PRIu32" (%s)", property, prop_name ? prop_name : "(null)");
			free(prop_name);
		}

		if (changed) {
			xsurface_set_net_wm_state(xsurface);
		}
	}
	// client_message->data.data32[3] is the source indication
	// all other values are set to 0

	if (fullscreen != xsurface->fullscreen) {
		wl_signal_emit_mutable(&xsurface->events.request_fullscreen, NULL);
	}

	if (maximized_vert != xsurface->maximized_vert
			|| maximized_horz != xsurface->maximized_horz) {
		wl_signal_emit_mutable(&xsurface->events.request_maximize, NULL);
	}

	if (minimized != xsurface->minimized) {
		struct wlr_xwayland_minimize_event minimize_event = {
			.surface = xsurface,
			.minimize = xsurface->minimized,
		};
		wl_signal_emit_mutable(&xsurface->events.request_minimize, &minimize_event);
	}

	if (sticky != xsurface->sticky) {
		wl_signal_emit_mutable(&xsurface->events.request_sticky, NULL);
	}

	if (shaded != xsurface->shaded) {
		wl_signal_emit_mutable(&xsurface->events.request_shaded, NULL);
	}

	if (skip_taskbar != xsurface->skip_taskbar) {
		wl_signal_emit_mutable(&xsurface->events.request_skip_taskbar, NULL);
	}

	if (skip_pager != xsurface->skip_pager) {
		wl_signal_emit_mutable(&xsurface->events.request_skip_pager, NULL);
	}

	if (above != xsurface->above) {
		wl_signal_emit_mutable(&xsurface->events.request_above, NULL);
	}

	if (below != xsurface->below) {
		wl_signal_emit_mutable(&xsurface->events.request_below, NULL);
	}

	if (demands_attention != xsurface->demands_attention) {
		wl_signal_emit_mutable(&xsurface->events.request_demands_attention, NULL);
	}
}

static void xwm_handle_wm_protocols_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *ev) {
	xcb_atom_t type = ev->data.data32[0];

	if (type == xwm->atoms[NET_WM_PING]) {
		xcb_window_t window_id = ev->data.data32[2];

		struct wlr_xwayland_surface *surface = lookup_surface(xwm, window_id);
		if (surface == NULL) {
			return;
		}

		if (!surface->pinging) {
			return;
		}

		wl_event_source_timer_update(surface->ping_timer, 0);
		surface->pinging = false;
	} else if (wlr_log_get_verbosity() >= WLR_DEBUG) {
		char *type_name = xwm_get_atom_name(xwm, type);
		wlr_log(WLR_DEBUG, "unhandled WM_PROTOCOLS client message %" PRIu32 " (%s)",
			type, type_name ? type_name : "(null)");
		free(type_name);
	}
}

static void xwm_handle_net_active_window_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *ev) {
	struct wlr_xwayland_surface *surface = lookup_surface(xwm, ev->window);
	if (surface == NULL) {
		return;
	}
	wl_signal_emit_mutable(&surface->events.request_activate, NULL);
}

static void xwm_handle_net_close_window_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *ev) {
	struct wlr_xwayland_surface *surface = lookup_surface(xwm, ev->window);
	if (surface == NULL) {
		return;
	}
	wl_signal_emit_mutable(&surface->events.request_close, NULL);
}

static void pending_startup_id_destroy(struct pending_startup_id *pending) {
	wl_list_remove(&pending->link);
	free(pending->msg);
	free(pending);
}

static void xwm_handle_net_startup_info_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *ev) {
	struct pending_startup_id *pending, *curr = NULL;
	wl_list_for_each(pending, &xwm->pending_startup_ids, link) {
		if (pending->window == ev->window) {
			curr = pending;
			break;
		}
	}

	char *start;
	size_t buf_len = sizeof(ev->data);
	if (curr) {
		curr->msg = realloc(curr->msg, curr->len + buf_len);
		if (!curr->msg) {
			pending_startup_id_destroy(curr);
			return;
		}
		start = curr->msg + curr->len;
		curr->len += buf_len;
	} else {
		curr = calloc(1, sizeof(*curr));
		if (!curr)
			return;
		curr->window = ev->window;
		curr->msg = malloc(buf_len);
		if (!curr->msg) {
			free(curr);
			return;
		}
		start = curr->msg;
		curr->len = buf_len;
		wl_list_insert(&xwm->pending_startup_ids, &curr->link);
	}

	char *id = NULL;
	const char *data = (const char *)ev->data.data8;
	for (size_t i = 0; i < buf_len; i++) {
		start[i] = data[i];
		if (start[i] == '\0') {
			if (strncmp(curr->msg, STARTUP_INFO_REMOVE_PREFIX,
					strlen(STARTUP_INFO_REMOVE_PREFIX)) == 0 &&
					strlen(curr->msg) > strlen(STARTUP_INFO_REMOVE_PREFIX)) {
				id = curr->msg + strlen(STARTUP_INFO_REMOVE_PREFIX);
				break;
			} else {
				wlr_log(WLR_ERROR, "Unhandled message '%s'\n", curr->msg);
				pending_startup_id_destroy(curr);
				return;
			}
		}
	}

	if (id) {
		struct wlr_xwayland_remove_startup_info_event data = { id, ev->window };
		wlr_log(WLR_DEBUG, "Got startup id: %s", id);
		wl_signal_emit_mutable(&xwm->xwayland->events.remove_startup_info, &data);
		pending_startup_id_destroy(curr);
	}
}

static void xwm_handle_wm_change_state_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *ev) {
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->window);
	uint32_t detail = ev->data.data32[0];

	if (xsurface == NULL) {
		return;
	}

	bool minimize;
	if (detail == XCB_ICCCM_WM_STATE_ICONIC) {
		minimize = true;
	} else if (detail == XCB_ICCCM_WM_STATE_NORMAL) {
		minimize = false;
	} else {
		wlr_log(WLR_DEBUG, "unhandled wm_change_state event %u", detail);
		return;
	}

	struct wlr_xwayland_minimize_event minimize_event = {
		.surface = xsurface,
		.minimize = minimize,
	};
	wl_signal_emit_mutable(&xsurface->events.request_minimize, &minimize_event);
}

static void xwm_handle_client_message(struct wlr_xwm *xwm,
		xcb_client_message_event_t *ev) {
	if (ev->type == xwm->atoms[WL_SURFACE_ID]) {
		xwm_handle_surface_id_message(xwm, ev);
	} else if (ev->type == xwm->atoms[WL_SURFACE_SERIAL]) {
		xwm_handle_surface_serial_message(xwm, ev);
	} else if (ev->type == xwm->atoms[NET_WM_STATE]) {
		xwm_handle_net_wm_state_message(xwm, ev);
	} else if (ev->type == xwm->atoms[NET_WM_MOVERESIZE]) {
		xwm_handle_net_wm_moveresize_message(xwm, ev);
	} else if (ev->type == xwm->atoms[WM_PROTOCOLS]) {
		xwm_handle_wm_protocols_message(xwm, ev);
	} else if (ev->type == xwm->atoms[NET_ACTIVE_WINDOW]) {
		xwm_handle_net_active_window_message(xwm, ev);
	} else if (ev->type == xwm->atoms[NET_CLOSE_WINDOW]) {
		xwm_handle_net_close_window_message(xwm, ev);
	} else if (ev->type == xwm->atoms[NET_STARTUP_INFO] ||
			ev->type == xwm->atoms[NET_STARTUP_INFO_BEGIN]) {
		xwm_handle_net_startup_info_message(xwm, ev);
	} else if (ev->type == xwm->atoms[WM_CHANGE_STATE]) {
		xwm_handle_wm_change_state_message(xwm, ev);
	} else if (!xwm_handle_selection_client_message(xwm, ev) &&
			wlr_log_get_verbosity() >= WLR_DEBUG) {
		char *type_name = xwm_get_atom_name(xwm, ev->type);
		wlr_log(WLR_DEBUG, "unhandled x11 client message %" PRIu32 " (%s)", ev->type,
			type_name ? type_name : "(null)");
		free(type_name);
	}
}

static bool validate_focus_serial(uint16_t last_focus_seq, uint16_t event_seq) {
	uint16_t rev_dist = event_seq - last_focus_seq;
	if (rev_dist >= UINT16_MAX / 2) {
		// Probably overflow or too old
		return false;
	}

	return true;
}

static void xwm_handle_focus_in(struct wlr_xwm *xwm,
		xcb_focus_in_event_t *ev) {
	// Ignore pointer focus change events
	if (ev->detail == XCB_NOTIFY_DETAIL_POINTER) {
		return;
	}

	// Do not interfere with keyboard grabs, but notify the
	// compositor. Note that many legitimate X11 applications use
	// keyboard grabs to "steal" focus for e.g. popup menus.
	struct wlr_xwayland_surface *xsurface = lookup_surface(xwm, ev->event);
	if (ev->mode == XCB_NOTIFY_MODE_GRAB) {
		if (xsurface) {
			wl_signal_emit_mutable(&xsurface->events.grab_focus, NULL);
		}
		return;
	}
	if (ev->mode == XCB_NOTIFY_MODE_UNGRAB) {
		/* Do we need an ungrab_focus event? */
		return;
	}

	// Ignore any out-of-date FocusIn event (older than the last
	// known WM-initiated focus change) to avoid race conditions.
	// https://github.com/swaywm/wlroots/issues/2324
	if (!validate_focus_serial(xwm->last_focus_seq, ev->sequence)) {
		return;
	}

	// Allow focus changes between surfaces belonging to the same
	// application. Steam for example relies on this:
	// https://github.com/swaywm/sway/issues/1865
	if (xsurface && ((xwm->focus_surface && xsurface->pid == xwm->focus_surface->pid) ||
			(xwm->offered_focus && xsurface->pid == xwm->offered_focus->pid))) {
		xwm_set_focused_window(xwm, xsurface);
		wl_signal_emit_mutable(&xsurface->events.focus_in, NULL);
	} else {
		// Try to prevent clients from changing focus between
		// applications, by refocusing the previous surface.
		xwm_focus_window(xwm, xwm->focus_surface);
	}
}

static void xwm_handle_xcb_error(struct wlr_xwm *xwm, xcb_value_error_t *ev) {
#if HAVE_XCB_ERRORS
	const char *major_name =
		xcb_errors_get_name_for_major_code(xwm->errors_context,
			ev->major_opcode);
	if (!major_name) {
		wlr_log(WLR_DEBUG, "xcb error happened, but could not get major name");
		goto log_raw;
	}

	const char *minor_name =
		xcb_errors_get_name_for_minor_code(xwm->errors_context,
			ev->major_opcode, ev->minor_opcode);

	const char *extension;
	const char *error_name =
		xcb_errors_get_name_for_error(xwm->errors_context,
			ev->error_code, &extension);
	if (!error_name) {
		wlr_log(WLR_DEBUG, "xcb error happened, but could not get error name");
		goto log_raw;
	}

	wlr_log(WLR_ERROR, "xcb error: op %s (%s), code %s (%s), sequence %"PRIu16", value %"PRIu32,
		major_name, minor_name ? minor_name : "no minor",
		error_name, extension ? extension : "no extension",
		ev->sequence, ev->bad_value);

	return;
log_raw:
#endif
	wlr_log(WLR_ERROR,
		"xcb error: op %"PRIu8":%"PRIu16", code %"PRIu8", sequence %"PRIu16", value %"PRIu32,
		ev->major_opcode, ev->minor_opcode, ev->error_code,
		ev->sequence, ev->bad_value);

}

static void xwm_handle_unhandled_event(struct wlr_xwm *xwm, xcb_generic_event_t *ev) {
#if HAVE_XCB_ERRORS
	const char *extension;
	const char *event_name =
		xcb_errors_get_name_for_xcb_event(xwm->errors_context,
			ev, &extension);
	if (!event_name) {
		wlr_log(WLR_DEBUG, "no name for unhandled event: %u",
			ev->response_type);
		return;
	}

	wlr_log(WLR_DEBUG, "unhandled X11 event: %s (%u)", event_name, ev->response_type);
#else
	wlr_log(WLR_DEBUG, "unhandled X11 event: %u", ev->response_type);
#endif
}

static int read_x11_events(struct wlr_xwm *xwm) {
	int count = 0;

	xcb_generic_event_t *event;
	while ((event = xcb_poll_for_event(xwm->xcb_conn))) {
		count++;

		if (xwm->xwayland->user_event_handler &&
				xwm->xwayland->user_event_handler(xwm->xwayland, event)) {
			free(event);
			continue;
		}

		if (xwm_handle_selection_event(xwm, event)) {
			free(event);
			continue;
		}

		switch (event->response_type & XCB_EVENT_RESPONSE_TYPE_MASK) {
		case XCB_CREATE_NOTIFY:
			xwm_handle_create_notify(xwm, (xcb_create_notify_event_t *)event);
			break;
		case XCB_DESTROY_NOTIFY:
			xwm_handle_destroy_notify(xwm, (xcb_destroy_notify_event_t *)event);
			break;
		case XCB_CONFIGURE_REQUEST:
			xwm_handle_configure_request(xwm,
				(xcb_configure_request_event_t *)event);
			break;
		case XCB_CONFIGURE_NOTIFY:
			xwm_handle_configure_notify(xwm,
				(xcb_configure_notify_event_t *)event);
			break;
		case XCB_MAP_REQUEST:
			xwm_handle_map_request(xwm, (xcb_map_request_event_t *)event);
			break;
		case XCB_MAP_NOTIFY:
			xwm_handle_map_notify(xwm, (xcb_map_notify_event_t *)event);
			break;
		case XCB_UNMAP_NOTIFY:
			xwm_handle_unmap_notify(xwm, (xcb_unmap_notify_event_t *)event);
			break;
		case XCB_PROPERTY_NOTIFY:
			xwm_handle_property_notify(xwm,
				(xcb_property_notify_event_t *)event);
			break;
		case XCB_CLIENT_MESSAGE:
			xwm_handle_client_message(xwm, (xcb_client_message_event_t *)event);
			break;
		case XCB_FOCUS_IN:
			xwm_handle_focus_in(xwm, (xcb_focus_in_event_t *)event);
			break;
		case 0:
			xwm_handle_xcb_error(xwm, (xcb_value_error_t *)event);
			break;
		default:
			xwm_handle_unhandled_event(xwm, event);
			break;
		}
		free(event);
	}

	return count;
}

static int x11_event_handler(int fd, uint32_t mask, void *data) {
	struct wlr_xwm *xwm = data;

	if ((mask & WL_EVENT_HANGUP) || (mask & WL_EVENT_ERROR)) {
		xwm_destroy(xwm);
		return 0;
	}

	int count = 0;
	if (mask & WL_EVENT_READABLE) {
		count = read_x11_events(xwm);
		if (count) {
			xwm_schedule_flush(xwm);
		}
	}

	if (mask & WL_EVENT_WRITABLE) {
		// xcb_flush() always blocks until it's written all pending requests,
		// but it's the only thing we have
		xcb_flush(xwm->xcb_conn);
		wl_event_source_fd_update(xwm->event_source, WL_EVENT_READABLE);
	}

	return count;
}

static void handle_compositor_new_surface(struct wl_listener *listener,
		void *data) {
	struct wlr_xwm *xwm =
		wl_container_of(listener, xwm, compositor_new_surface);
	struct wlr_surface *surface = data;

	struct wl_client *client = wl_resource_get_client(surface->resource);
	if (client != xwm->xwayland->server->client) {
		return;
	}

	wlr_log(WLR_DEBUG, "New xwayland surface: %p", surface);

	uint32_t surface_id = wl_resource_get_id(surface->resource);
	struct wlr_xwayland_surface *xsurface;
	wl_list_for_each(xsurface, &xwm->unpaired_surfaces, unpaired_link) {
		if (xsurface->surface_id == surface_id) {
			xwayland_surface_associate(xwm, xsurface, surface);
			xwm_schedule_flush(xwm);
			return;
		}
	}
}

static void handle_compositor_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_xwm *xwm =
		wl_container_of(listener, xwm, compositor_destroy);
	wl_list_remove(&xwm->compositor_new_surface.link);
	wl_list_remove(&xwm->compositor_destroy.link);
	wl_list_init(&xwm->compositor_new_surface.link);
	wl_list_init(&xwm->compositor_destroy.link);
}

static void handle_shell_v1_new_surface(struct wl_listener *listener,
		void *data) {
	struct wlr_xwm *xwm = wl_container_of(listener, xwm, shell_v1_new_surface);
	struct wlr_xwayland_surface_v1 *shell_surface = data;

	struct wlr_xwayland_surface *xsurface;
	wl_list_for_each(xsurface, &xwm->unpaired_surfaces, unpaired_link) {
		if (xsurface->serial == shell_surface->serial) {
			xwayland_surface_associate(xwm, xsurface, shell_surface->surface);
			return;
		}
	}
}

static void handle_shell_v1_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_xwm *xwm =
		wl_container_of(listener, xwm, shell_v1_destroy);
	wl_list_remove(&xwm->shell_v1_new_surface.link);
	wl_list_remove(&xwm->shell_v1_destroy.link);
	wl_list_init(&xwm->shell_v1_new_surface.link);
	wl_list_init(&xwm->shell_v1_destroy.link);
}

void wlr_xwayland_surface_activate(struct wlr_xwayland_surface *xsurface,
		bool activated) {
	struct wlr_xwayland_surface *focused = xsurface->xwm->focus_surface;
	if (activated) {
		xwm_surface_activate(xsurface->xwm, xsurface);
	} else if (focused == xsurface) {
		xwm_surface_activate(xsurface->xwm, NULL);
	}
}

void wlr_xwayland_surface_configure(struct wlr_xwayland_surface *xsurface,
		int16_t x, int16_t y, uint16_t width, uint16_t height) {
	int old_w = xsurface->width;
	int old_h = xsurface->height;

	xsurface->x = x;
	xsurface->y = y;
	xsurface->width = width;
	xsurface->height = height;

	struct wlr_xwm *xwm = xsurface->xwm;
	uint32_t mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y |
		XCB_CONFIG_WINDOW_WIDTH | XCB_CONFIG_WINDOW_HEIGHT |
		XCB_CONFIG_WINDOW_BORDER_WIDTH;
	uint32_t values[] = {x, y, width, height, 0};
	xcb_configure_window(xwm->xcb_conn, xsurface->window_id, mask, values);

	// If the window size did not change, then we cannot rely on
	// the X server to generate a ConfigureNotify event. Instead,
	// we are supposed to send a synthetic event. See ICCCM part
	// 4.1.5. But we ignore override-redirect windows as ICCCM does
	// not apply to them.
	if (width == old_w && height == old_h && !xsurface->override_redirect) {
		xcb_configure_notify_event_t configure_notify = {
			.response_type = XCB_CONFIGURE_NOTIFY,
			.event = xsurface->window_id,
			.window = xsurface->window_id,
			.x = x,
			.y = y,
			.width = width,
			.height = height,
		};

		xwm_send_event_with_size(xwm->xcb_conn, 0, xsurface->window_id,
			XCB_EVENT_MASK_STRUCTURE_NOTIFY,
			&configure_notify,
			sizeof(configure_notify));
	}

	xwm_schedule_flush(xwm);
}

void wlr_xwayland_surface_close(struct wlr_xwayland_surface *xsurface) {
	struct wlr_xwm *xwm = xsurface->xwm;

	bool supports_delete = false;
	for (size_t i = 0; i < xsurface->protocols_len; i++) {
		if (xsurface->protocols[i] == xwm->atoms[WM_DELETE_WINDOW]) {
			supports_delete = true;
			break;
		}
	}

	if (supports_delete) {
		xcb_client_message_data_t message_data = {0};
		message_data.data32[0] = xwm->atoms[WM_DELETE_WINDOW];
		message_data.data32[1] = XCB_CURRENT_TIME;
		xwm_send_wm_message(xsurface, &message_data, XCB_EVENT_MASK_NO_EVENT);
	} else {
		xcb_kill_client(xwm->xcb_conn, xsurface->window_id);
		xwm_schedule_flush(xwm);
	}
}

void xwm_destroy(struct wlr_xwm *xwm) {
	if (!xwm) {
		return;
	}

	xwm_selection_finish(&xwm->clipboard_selection);
	xwm_selection_finish(&xwm->primary_selection);
	xwm_selection_finish(&xwm->dnd_selection);

	xwm_seat_unlink_drag_handlers(xwm);

	if (xwm->seat) {
		if (xwm->seat->selection_source &&
				data_source_is_xwayland(xwm->seat->selection_source)) {
			wlr_seat_set_selection(xwm->seat, NULL,
				wl_display_next_serial(xwm->xwayland->wl_display));
		}

		if (xwm->seat->primary_selection_source &&
				primary_selection_source_is_xwayland(
					xwm->seat->primary_selection_source)) {
			wlr_seat_set_primary_selection(xwm->seat, NULL,
				wl_display_next_serial(xwm->xwayland->wl_display));
		}

		wlr_xwayland_set_seat(xwm->xwayland, NULL);
	}

	if (xwm->cursor) {
		xcb_free_cursor(xwm->xcb_conn, xwm->cursor);
	}
	if (xwm->colormap) {
		xcb_free_colormap(xwm->xcb_conn, xwm->colormap);
	}
	if (xwm->no_focus_window) {
		xcb_destroy_window(xwm->xcb_conn, xwm->no_focus_window);
	}
	if (xwm->window) {
		xcb_destroy_window(xwm->xcb_conn, xwm->window);
	}
	if (xwm->event_source) {
		wl_event_source_remove(xwm->event_source);
	}
#if HAVE_XCB_ERRORS
	if (xwm->errors_context) {
		xcb_errors_context_free(xwm->errors_context);
	}
#endif
	struct wlr_xwayland_surface *xsurface, *tmp;
	wl_list_for_each_safe(xsurface, tmp, &xwm->surfaces, link) {
		xwayland_surface_destroy(xsurface);
	}
	wl_list_for_each_safe(xsurface, tmp, &xwm->unpaired_surfaces, unpaired_link) {
		xwayland_surface_destroy(xsurface);
	}
	wl_list_remove(&xwm->compositor_new_surface.link);
	wl_list_remove(&xwm->compositor_destroy.link);
	wl_list_remove(&xwm->shell_v1_new_surface.link);
	wl_list_remove(&xwm->shell_v1_destroy.link);
	xcb_disconnect(xwm->xcb_conn);

	struct pending_startup_id *pending, *next;
	wl_list_for_each_safe(pending, next, &xwm->pending_startup_ids, link) {
		pending_startup_id_destroy(pending);
	}

	xwm->xwayland->xwm = NULL;
	free(xwm);
}

static void xwm_get_resources(struct wlr_xwm *xwm) {
	xcb_prefetch_extension_data(xwm->xcb_conn, &xcb_xfixes_id);
	xcb_prefetch_extension_data(xwm->xcb_conn, &xcb_composite_id);
	xcb_prefetch_extension_data(xwm->xcb_conn, &xcb_res_id);

	size_t i;
	xcb_intern_atom_cookie_t cookies[ATOM_LAST];

	for (i = 0; i < ATOM_LAST; i++) {
		cookies[i] =
			xcb_intern_atom(xwm->xcb_conn, 0, strlen(atom_map[i]), atom_map[i]);
	}
	for (i = 0; i < ATOM_LAST; i++) {
		xcb_generic_error_t *error;
		xcb_intern_atom_reply_t *reply =
			xcb_intern_atom_reply(xwm->xcb_conn, cookies[i], &error);
		if (reply && !error) {
			xwm->atoms[i] = reply->atom;
		}
		free(reply);

		if (error) {
			wlr_log(WLR_ERROR, "could not resolve atom %s, x11 error code %d",
				atom_map[i], error->error_code);
			free(error);
			return;
		}
	}

	xwm->xfixes = xcb_get_extension_data(xwm->xcb_conn, &xcb_xfixes_id);

	if (!xwm->xfixes || !xwm->xfixes->present) {
		wlr_log(WLR_DEBUG, "xfixes not available");
	}

	xcb_xfixes_query_version_cookie_t xfixes_cookie;
	xcb_xfixes_query_version_reply_t *xfixes_reply;
	xfixes_cookie =
		xcb_xfixes_query_version(xwm->xcb_conn, XCB_XFIXES_MAJOR_VERSION,
			XCB_XFIXES_MINOR_VERSION);
	xfixes_reply =
		xcb_xfixes_query_version_reply(xwm->xcb_conn, xfixes_cookie, NULL);

	wlr_log(WLR_DEBUG, "xfixes version: %" PRIu32 ".%" PRIu32,
		xfixes_reply->major_version, xfixes_reply->minor_version);
	xwm->xfixes_major_version = xfixes_reply->major_version;

	free(xfixes_reply);

	const xcb_query_extension_reply_t *xres =
		xcb_get_extension_data(xwm->xcb_conn, &xcb_res_id);
	if (!xres || !xres->present) {
		return;
	}

	xcb_res_query_version_cookie_t xres_cookie =
		xcb_res_query_version(xwm->xcb_conn, XCB_RES_MAJOR_VERSION,
			XCB_RES_MINOR_VERSION);
	xcb_res_query_version_reply_t *xres_reply =
		xcb_res_query_version_reply(xwm->xcb_conn, xres_cookie, NULL);
	if (xres_reply == NULL) {
		return;
	}

	wlr_log(WLR_DEBUG, "xres version: %" PRIu32 ".%" PRIu32,
		xres_reply->server_major, xres_reply->server_minor);
	if (xres_reply->server_major > 1 ||
			(xres_reply->server_major == 1 && xres_reply->server_minor >= 2)) {
		xwm->xres = xres;
	}
	free(xres_reply);
}

static void xwm_create_wm_window(struct wlr_xwm *xwm) {
	static const char name[] = "wlroots wm";

	xwm->window = xcb_generate_id(xwm->xcb_conn);

	xcb_create_window(xwm->xcb_conn,
		XCB_COPY_FROM_PARENT,
		xwm->window,
		xwm->screen->root,
		0, 0,
		10, 10,
		0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		xwm->screen->root_visual,
		0, NULL);

	xcb_change_property(xwm->xcb_conn,
		XCB_PROP_MODE_REPLACE,
		xwm->window,
		xwm->atoms[NET_WM_NAME],
		xwm->atoms[UTF8_STRING],
		8, // format
		strlen(name), name);

	xcb_change_property(xwm->xcb_conn,
		XCB_PROP_MODE_REPLACE,
		xwm->screen->root,
		xwm->atoms[NET_SUPPORTING_WM_CHECK],
		XCB_ATOM_WINDOW,
		32, // format
		1, &xwm->window);

	xcb_change_property(xwm->xcb_conn,
		XCB_PROP_MODE_REPLACE,
		xwm->window,
		xwm->atoms[NET_SUPPORTING_WM_CHECK],
		XCB_ATOM_WINDOW,
		32, // format
		1, &xwm->window);

	xcb_set_selection_owner(xwm->xcb_conn,
		xwm->window,
		xwm->atoms[WM_S0],
		XCB_CURRENT_TIME);

	xcb_set_selection_owner(xwm->xcb_conn,
		xwm->window,
		xwm->atoms[NET_WM_CM_S0],
		XCB_CURRENT_TIME);
}

static void xwm_create_no_focus_window(struct wlr_xwm *xwm) {
	xwm->no_focus_window = xcb_generate_id(xwm->xcb_conn);

	uint32_t values[2] = {
		1,
		XCB_EVENT_MASK_KEY_PRESS |
			XCB_EVENT_MASK_KEY_RELEASE |
			XCB_EVENT_MASK_FOCUS_CHANGE
	};
	xcb_create_window(xwm->xcb_conn,
		XCB_COPY_FROM_PARENT,
		xwm->no_focus_window,
		xwm->screen->root,
		-100, -100,
		1, 1,
		0,
		XCB_WINDOW_CLASS_COPY_FROM_PARENT,
		XCB_COPY_FROM_PARENT,
		XCB_CW_OVERRIDE_REDIRECT |
			XCB_CW_EVENT_MASK,
		values);

	xcb_map_window(xwm->xcb_conn, xwm->no_focus_window);
}

// TODO use me to support 32 bit color somehow
static void xwm_get_visual_and_colormap(struct wlr_xwm *xwm) {
	xcb_depth_iterator_t d_iter;
	xcb_visualtype_iterator_t vt_iter;
	xcb_visualtype_t *visualtype;

	d_iter = xcb_screen_allowed_depths_iterator(xwm->screen);
	visualtype = NULL;
	while (d_iter.rem > 0) {
		if (d_iter.data->depth == 32) {
			vt_iter = xcb_depth_visuals_iterator(d_iter.data);
			visualtype = vt_iter.data;
			break;
		}

		xcb_depth_next(&d_iter);
	}

	if (visualtype == NULL) {
		wlr_log(WLR_DEBUG, "No 32 bit visualtype\n");
		return;
	}

	xwm->visual_id = visualtype->visual_id;
	xwm->colormap = xcb_generate_id(xwm->xcb_conn);
	xcb_create_colormap(xwm->xcb_conn,
		XCB_COLORMAP_ALLOC_NONE,
		xwm->colormap,
		xwm->screen->root,
		xwm->visual_id);
}

static void xwm_get_render_format(struct wlr_xwm *xwm) {
	xcb_render_query_pict_formats_cookie_t cookie =
		xcb_render_query_pict_formats(xwm->xcb_conn);
	xcb_render_query_pict_formats_reply_t *reply =
		xcb_render_query_pict_formats_reply(xwm->xcb_conn, cookie, NULL);
	if (!reply) {
		wlr_log(WLR_ERROR, "Did not get any reply from xcb_render_query_pict_formats");
		return;
	}
	xcb_render_pictforminfo_iterator_t iter =
		xcb_render_query_pict_formats_formats_iterator(reply);
	xcb_render_pictforminfo_t *format = NULL;
	while (iter.rem > 0) {
		if (iter.data->depth == 32) {
			format = iter.data;
			break;
		}

		xcb_render_pictforminfo_next(&iter);
	}

	if (format == NULL) {
		wlr_log(WLR_DEBUG, "No 32 bit render format");
		free(reply);
		return;
	}

	xwm->render_format_id = format->id;
	free(reply);
}

void xwm_set_cursor(struct wlr_xwm *xwm, const uint8_t *pixels, uint32_t stride,
		uint32_t width, uint32_t height, int32_t hotspot_x, int32_t hotspot_y) {
	if (!xwm->render_format_id) {
		wlr_log(WLR_ERROR, "Cannot set xwm cursor: no render format available");
		return;
	}
	if (xwm->cursor) {
		xcb_free_cursor(xwm->xcb_conn, xwm->cursor);
	}

	int depth = 32;

	xcb_pixmap_t pix = xcb_generate_id(xwm->xcb_conn);
	xcb_create_pixmap(xwm->xcb_conn, depth, pix, xwm->screen->root, width,
		height);

	xcb_render_picture_t pic = xcb_generate_id(xwm->xcb_conn);
	xcb_render_create_picture(xwm->xcb_conn, pic, pix, xwm->render_format_id,
		0, 0);

	xcb_gcontext_t gc = xcb_generate_id(xwm->xcb_conn);
	xcb_create_gc(xwm->xcb_conn, gc, pix, 0, NULL);

	xcb_put_image(xwm->xcb_conn, XCB_IMAGE_FORMAT_Z_PIXMAP, pix, gc,
		width, height, 0, 0, 0, depth, stride * height * sizeof(uint8_t),
		pixels);
	xcb_free_gc(xwm->xcb_conn, gc);

	xwm->cursor = xcb_generate_id(xwm->xcb_conn);
	xcb_render_create_cursor(xwm->xcb_conn, xwm->cursor, pic, hotspot_x,
		hotspot_y);
	xcb_free_pixmap(xwm->xcb_conn, pix);
	xcb_render_free_picture(xwm->xcb_conn, pic);

	uint32_t values[] = {xwm->cursor};
	xcb_change_window_attributes(xwm->xcb_conn, xwm->screen->root,
		XCB_CW_CURSOR, values);
	xwm_schedule_flush(xwm);
}

struct wlr_xwm *xwm_create(struct wlr_xwayland *xwayland, int wm_fd) {
	struct wlr_xwm *xwm = calloc(1, sizeof(*xwm));
	if (xwm == NULL) {
		close(wm_fd);
		return NULL;
	}

	xwm->xwayland = xwayland;
	wl_list_init(&xwm->surfaces);
	wl_list_init(&xwm->surfaces_in_stack_order);
	wl_list_init(&xwm->unpaired_surfaces);
	wl_list_init(&xwm->pending_startup_ids);
	wl_list_init(&xwm->seat_drag_source_destroy.link);
	wl_list_init(&xwm->drag_focus_destroy.link);
	wl_list_init(&xwm->drop_focus_destroy.link);

	xwm->ping_timeout = 10000;

	// xcb_connect_to_fd takes ownership of the FD regardless of success/failure
	xwm->xcb_conn = xcb_connect_to_fd(wm_fd, NULL);

	int rc = xcb_connection_has_error(xwm->xcb_conn);
	if (rc) {
		wlr_log(WLR_ERROR, "xcb connect failed: %d", rc);
		xcb_disconnect(xwm->xcb_conn);
		free(xwm);
		return NULL;
	}

#if HAVE_XCB_ERRORS
	if (xcb_errors_context_new(xwm->xcb_conn, &xwm->errors_context)) {
		wlr_log(WLR_ERROR, "Could not allocate error context");
		xwm_destroy(xwm);
		return NULL;
	}
#endif

	xcb_screen_iterator_t screen_iterator =
		xcb_setup_roots_iterator(xcb_get_setup(xwm->xcb_conn));
	xwm->screen = screen_iterator.data;

	struct wl_event_loop *event_loop =
		wl_display_get_event_loop(xwayland->wl_display);
	xwm->event_source = wl_event_loop_add_fd(event_loop, wm_fd,
		WL_EVENT_READABLE, x11_event_handler, xwm);
	wl_event_source_check(xwm->event_source);

	xwm_get_resources(xwm);
	xwm_get_visual_and_colormap(xwm);
	xwm_get_render_format(xwm);

	uint32_t values[] = {
		XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
			XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
			XCB_EVENT_MASK_PROPERTY_CHANGE,
	};
	xcb_change_window_attributes(xwm->xcb_conn,
		xwm->screen->root,
		XCB_CW_EVENT_MASK,
		values);

	xcb_composite_redirect_subwindows(xwm->xcb_conn,
		xwm->screen->root,
		XCB_COMPOSITE_REDIRECT_MANUAL);

	xcb_atom_t supported[] = {
		xwm->atoms[NET_WM_STATE],
		xwm->atoms[NET_ACTIVE_WINDOW],
		xwm->atoms[NET_CLOSE_WINDOW],
		xwm->atoms[NET_WM_MOVERESIZE],
		xwm->atoms[NET_WM_STATE_FOCUSED],
		xwm->atoms[NET_WM_STATE_MODAL],
		xwm->atoms[NET_WM_STATE_FULLSCREEN],
		xwm->atoms[NET_WM_STATE_MAXIMIZED_VERT],
		xwm->atoms[NET_WM_STATE_MAXIMIZED_HORZ],
		xwm->atoms[NET_WM_STATE_HIDDEN],
		xwm->atoms[NET_WM_STATE_STICKY],
		xwm->atoms[NET_WM_STATE_SHADED],
		xwm->atoms[NET_WM_STATE_SKIP_TASKBAR],
		xwm->atoms[NET_WM_STATE_SKIP_PAGER],
		xwm->atoms[NET_WM_STATE_ABOVE],
		xwm->atoms[NET_WM_STATE_BELOW],
		xwm->atoms[NET_WM_STATE_DEMANDS_ATTENTION],
		xwm->atoms[NET_CLIENT_LIST],
		xwm->atoms[NET_CLIENT_LIST_STACKING],
	};
	xcb_change_property(xwm->xcb_conn,
		XCB_PROP_MODE_REPLACE,
		xwm->screen->root,
		xwm->atoms[NET_SUPPORTED],
		XCB_ATOM_ATOM,
		32,
		sizeof(supported)/sizeof(*supported),
		supported);

	if (xwm->xwayland->server->options.terminate_delay > 0 &&
			xwm->xfixes_major_version >= 6) {
		xcb_xfixes_set_client_disconnect_mode(xwm->xcb_conn,
			XCB_XFIXES_CLIENT_DISCONNECT_FLAGS_TERMINATE);
	}

	xcb_flush(xwm->xcb_conn);

	xwm_set_net_active_window(xwm, XCB_WINDOW_NONE);

	xwm_selection_init(&xwm->clipboard_selection, xwm, xwm->atoms[CLIPBOARD]);
	xwm_selection_init(&xwm->primary_selection, xwm, xwm->atoms[PRIMARY]);
	xwm_selection_init(&xwm->dnd_selection, xwm, xwm->atoms[DND_SELECTION]);

	xwm->compositor_new_surface.notify = handle_compositor_new_surface;
	wl_signal_add(&xwayland->compositor->events.new_surface,
		&xwm->compositor_new_surface);
	xwm->compositor_destroy.notify = handle_compositor_destroy;
	wl_signal_add(&xwayland->compositor->events.destroy,
		&xwm->compositor_destroy);

	xwm->shell_v1_new_surface.notify = handle_shell_v1_new_surface;
	wl_signal_add(&xwayland->shell_v1->events.new_surface,
		&xwm->shell_v1_new_surface);
	xwm->shell_v1_destroy.notify = handle_shell_v1_destroy;
	wl_signal_add(&xwayland->shell_v1->events.destroy,
		&xwm->shell_v1_destroy);

	xwm_create_wm_window(xwm);
	xwm_create_no_focus_window(xwm);

	xcb_flush(xwm->xcb_conn);

	return xwm;
}

void wlr_xwayland_surface_set_withdrawn(struct wlr_xwayland_surface *surface,
		bool withdrawn) {
	surface->withdrawn = withdrawn;
	xsurface_set_wm_state(surface);
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_minimized(struct wlr_xwayland_surface *surface,
		bool minimized) {
	surface->minimized = minimized;
	xsurface_set_wm_state(surface);
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_maximized(struct wlr_xwayland_surface *surface,
		bool maximized_horz, bool maximized_vert) {
	surface->maximized_horz = maximized_horz;
	surface->maximized_vert = maximized_vert;
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_fullscreen(struct wlr_xwayland_surface *surface,
		bool fullscreen) {
	surface->fullscreen = fullscreen;
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_sticky(struct wlr_xwayland_surface *surface, bool sticky) {
	surface->sticky = sticky;
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_shaded(struct wlr_xwayland_surface *surface, bool shaded) {
	surface->shaded = shaded;
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_skip_taskbar(struct wlr_xwayland_surface *surface,
		bool skip_taskbar) {
	surface->skip_taskbar = skip_taskbar;
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_skip_pager(struct wlr_xwayland_surface *surface,
		bool skip_pager) {
	surface->skip_pager = skip_pager;
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_above(struct wlr_xwayland_surface *surface, bool above) {
	surface->above = above;
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_below(struct wlr_xwayland_surface *surface, bool below) {
	surface->below = below;
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

void wlr_xwayland_surface_set_demands_attention(struct wlr_xwayland_surface *surface,
		bool demands_attention) {
	surface->demands_attention = demands_attention;
	xsurface_set_net_wm_state(surface);
	xwm_schedule_flush(surface->xwm);
}

bool xwm_atoms_contains(struct wlr_xwm *xwm, xcb_atom_t *atoms,
		size_t num_atoms, enum atom_name needle) {
	xcb_atom_t atom = xwm->atoms[needle];

	for (size_t i = 0; i < num_atoms; ++i) {
		if (atom == atoms[i]) {
			return true;
		}
	}

	return false;
}

void wlr_xwayland_surface_ping(struct wlr_xwayland_surface *surface) {
	xcb_client_message_data_t data = { 0 };
	data.data32[0] = surface->xwm->atoms[NET_WM_PING];
	data.data32[1] = XCB_CURRENT_TIME;
	data.data32[2] = surface->window_id;

	xwm_send_wm_message(surface, &data, XCB_EVENT_MASK_NO_EVENT);

	wl_event_source_timer_update(surface->ping_timer,
		surface->xwm->ping_timeout);
	surface->pinging = true;
}

bool wlr_xwayland_surface_has_window_type(
		const struct wlr_xwayland_surface *xsurface,
		enum wlr_xwayland_net_wm_window_type window_type) {
	static const enum atom_name atom_names[] = {
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DESKTOP]       = NET_WM_WINDOW_TYPE_DESKTOP,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DOCK]          = NET_WM_WINDOW_TYPE_DOCK,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLBAR]       = NET_WM_WINDOW_TYPE_TOOLBAR,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_MENU]          = NET_WM_WINDOW_TYPE_MENU,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_UTILITY]       = NET_WM_WINDOW_TYPE_UTILITY,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_SPLASH]        = NET_WM_WINDOW_TYPE_SPLASH,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DIALOG]        = NET_WM_WINDOW_TYPE_DIALOG,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DROPDOWN_MENU] = NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_POPUP_MENU]    = NET_WM_WINDOW_TYPE_POPUP_MENU,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_TOOLTIP]       = NET_WM_WINDOW_TYPE_TOOLTIP,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_NOTIFICATION]  = NET_WM_WINDOW_TYPE_NOTIFICATION,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_COMBO]         = NET_WM_WINDOW_TYPE_COMBO,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_DND]           = NET_WM_WINDOW_TYPE_DND,
		[WLR_XWAYLAND_NET_WM_WINDOW_TYPE_NORMAL]        = NET_WM_WINDOW_TYPE_NORMAL,
	};

	if (window_type >= 0 && window_type < sizeof(atom_names) / sizeof(atom_names[0])) {
		return xwm_atoms_contains(xsurface->xwm, xsurface->window_type,
			xsurface->window_type_len, atom_names[window_type]);
	}

	return false;
}

bool wlr_xwayland_surface_override_redirect_wants_focus(
		const struct wlr_xwayland_surface *xsurface) {
	static const enum atom_name needles[] = {
		NET_WM_WINDOW_TYPE_COMBO,
		NET_WM_WINDOW_TYPE_DND,
		NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
		NET_WM_WINDOW_TYPE_MENU,
		NET_WM_WINDOW_TYPE_NOTIFICATION,
		NET_WM_WINDOW_TYPE_POPUP_MENU,
		NET_WM_WINDOW_TYPE_SPLASH,
		NET_WM_WINDOW_TYPE_DESKTOP,
		NET_WM_WINDOW_TYPE_TOOLTIP,
		NET_WM_WINDOW_TYPE_UTILITY,
	};

	for (size_t i = 0; i < sizeof(needles) / sizeof(needles[0]); ++i) {
		if (xwm_atoms_contains(xsurface->xwm, xsurface->window_type,
				xsurface->window_type_len, needles[i])) {
			return false;
		}
	}

	return true;
}

enum wlr_xwayland_icccm_input_model wlr_xwayland_surface_icccm_input_model(
		const struct wlr_xwayland_surface *xsurface) {
	bool take_focus = xwm_atoms_contains(xsurface->xwm,
		xsurface->protocols, xsurface->protocols_len,
		WM_TAKE_FOCUS);

	if (!xsurface->hints || xsurface->hints->input) {
		if (take_focus) {
			return WLR_ICCCM_INPUT_MODEL_LOCAL;
		}
		return WLR_ICCCM_INPUT_MODEL_PASSIVE;
	} else {
		if (take_focus) {
			return WLR_ICCCM_INPUT_MODEL_GLOBAL;
		}
	}
	return WLR_ICCCM_INPUT_MODEL_NONE;
}

void wlr_xwayland_set_workareas(struct wlr_xwayland *wlr_xwayland,
		const struct wlr_box *workareas, size_t num_workareas) {
	uint32_t *data = malloc(4 * sizeof(uint32_t) * num_workareas);
	if (!data) {
		return;
	}

	for (size_t i = 0; i < num_workareas; i++) {
		data[4 * i] = workareas[i].x;
		data[4 * i + 1] = workareas[i].y;
		data[4 * i + 2] = workareas[i].width;
		data[4 * i + 3] = workareas[i].height;
	}

	struct wlr_xwm *xwm = wlr_xwayland->xwm;
	xcb_change_property(xwm->xcb_conn, XCB_PROP_MODE_REPLACE,
			xwm->screen->root, xwm->atoms[NET_WORKAREA],
			XCB_ATOM_CARDINAL, 32, 4 * num_workareas, data);
	free(data);
}

xcb_connection_t *wlr_xwayland_get_xwm_connection(
	struct wlr_xwayland *wlr_xwayland) {
	return wlr_xwayland->xwm ? wlr_xwayland->xwm->xcb_conn : NULL;
}

void xwm_schedule_flush(struct wlr_xwm *xwm) {
	wl_event_source_fd_update(xwm->event_source, WL_EVENT_READABLE | WL_EVENT_WRITABLE);
}
