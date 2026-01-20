#ifndef XWAYLAND_XWM_H
#define XWAYLAND_XWM_H

#include <wayland-server-core.h>
#include <wlr/config.h>
#include <wlr/xwayland.h>
#include <xcb/render.h>
#include "config.h"
#include "xwayland/selection.h"

#if HAVE_XCB_ERRORS
#include <xcb/xcb_errors.h>
#endif

/* This is in xcb/xcb_event.h, but pulling xcb-util just for a constant
 * others redefine anyway is meh
 */
#define XCB_EVENT_RESPONSE_TYPE_MASK (0x7f)

enum atom_name {
	WL_SURFACE_ID,
	WL_SURFACE_SERIAL,
	WM_DELETE_WINDOW,
	WM_PROTOCOLS,
	WM_HINTS,
	WM_NORMAL_HINTS,
	WM_SIZE_HINTS,
	WM_WINDOW_ROLE,
	MOTIF_WM_HINTS,
	UTF8_STRING,
	WM_S0,
	NET_SUPPORTED,
	NET_WM_CM_S0,
	NET_WM_PID,
	NET_WM_NAME,
	NET_WM_STATE,
	NET_WM_STRUT_PARTIAL,
	NET_WM_WINDOW_TYPE,
	NET_WM_ICON,
	WM_TAKE_FOCUS,
	WINDOW,
	NET_ACTIVE_WINDOW,
	NET_CLOSE_WINDOW,
	NET_WM_MOVERESIZE,
	NET_SUPPORTING_WM_CHECK,
	NET_WM_STATE_FOCUSED,
	NET_WM_STATE_MODAL,
	NET_WM_STATE_FULLSCREEN,
	NET_WM_STATE_MAXIMIZED_VERT,
	NET_WM_STATE_MAXIMIZED_HORZ,
	NET_WM_STATE_HIDDEN,
	NET_WM_STATE_STICKY,
	NET_WM_STATE_SHADED,
	NET_WM_STATE_SKIP_TASKBAR,
	NET_WM_STATE_SKIP_PAGER,
	NET_WM_STATE_ABOVE,
	NET_WM_STATE_BELOW,
	NET_WM_STATE_DEMANDS_ATTENTION,
	NET_WM_PING,
	WM_CHANGE_STATE,
	WM_STATE,
	CLIPBOARD,
	PRIMARY,
	WL_SELECTION,
	TARGETS,
	CLIPBOARD_MANAGER,
	INCR,
	TEXT,
	TIMESTAMP,
	DELETE,
	NET_STARTUP_ID,
	NET_STARTUP_INFO,
	NET_STARTUP_INFO_BEGIN,
	NET_WM_WINDOW_OPACITY,
	NET_WM_WINDOW_TYPE_NORMAL,
	NET_WM_WINDOW_TYPE_UTILITY,
	NET_WM_WINDOW_TYPE_TOOLTIP,
	NET_WM_WINDOW_TYPE_DND,
	NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
	NET_WM_WINDOW_TYPE_POPUP_MENU,
	NET_WM_WINDOW_TYPE_COMBO,
	NET_WM_WINDOW_TYPE_MENU,
	NET_WM_WINDOW_TYPE_NOTIFICATION,
	NET_WM_WINDOW_TYPE_SPLASH,
	NET_WM_WINDOW_TYPE_DESKTOP,
	NET_WM_WINDOW_TYPE_DOCK,
	NET_WM_WINDOW_TYPE_TOOLBAR,
	NET_WM_WINDOW_TYPE_DIALOG,
	DND_SELECTION,
	DND_AWARE,
	DND_STATUS,
	DND_POSITION,
	DND_ENTER,
	DND_LEAVE,
	DND_DROP,
	DND_FINISHED,
	DND_PROXY,
	DND_TYPE_LIST,
	DND_ACTION_MOVE,
	DND_ACTION_COPY,
	DND_ACTION_ASK,
	DND_ACTION_PRIVATE,
	NET_CLIENT_LIST,
	NET_CLIENT_LIST_STACKING,
	NET_WORKAREA,
	ATOM_LAST // keep last
};

struct wlr_xwm {
	struct wlr_xwayland *xwayland;
	struct wl_event_source *event_source;
	struct wlr_seat *seat;
	uint32_t ping_timeout;

	xcb_atom_t atoms[ATOM_LAST];
	xcb_connection_t *xcb_conn;
	xcb_screen_t *screen;
	xcb_window_t window;
	xcb_window_t no_focus_window;
	xcb_visualid_t visual_id;
	xcb_colormap_t colormap;
	xcb_render_pictformat_t render_format_id;
	xcb_cursor_t cursor;

	struct wlr_xwm_selection clipboard_selection;
	struct wlr_xwm_selection primary_selection;
	struct wlr_xwm_selection dnd_selection;

	struct wlr_xwayland_surface *focus_surface;
	struct wlr_xwayland_surface *offered_focus;

	// Surfaces in creation order
	struct wl_list surfaces; // wlr_xwayland_surface.link
	// Surfaces in bottom-to-top stacking order, for _NET_CLIENT_LIST_STACKING
	struct wl_list surfaces_in_stack_order; // wlr_xwayland_surface.stack_link
	struct wl_list unpaired_surfaces; // wlr_xwayland_surface.unpaired_link
	struct wl_list pending_startup_ids; // pending_startup_id

	struct wlr_drag *drag;
	struct wlr_xwayland_surface *drag_focus;
	struct wlr_xwayland_surface *drop_focus;

	const xcb_query_extension_reply_t *xfixes;
	const xcb_query_extension_reply_t *xres;
	uint32_t xfixes_major_version;
#if HAVE_XCB_ERRORS
	xcb_errors_context_t *errors_context;
#endif
	unsigned int last_focus_seq;

	struct wl_listener compositor_new_surface;
	struct wl_listener compositor_destroy;
	struct wl_listener shell_v1_new_surface;
	struct wl_listener shell_v1_destroy;
	struct wl_listener seat_set_selection;
	struct wl_listener seat_set_primary_selection;
	struct wl_listener seat_start_drag;
	struct wl_listener seat_drag_focus;
	struct wl_listener seat_drag_motion;
	struct wl_listener seat_drag_drop;
	struct wl_listener seat_drag_destroy;
	struct wl_listener seat_drag_source_destroy;
	struct wl_listener drag_focus_destroy;
	struct wl_listener drop_focus_destroy;
};

// xwm_create takes ownership of wm_fd and will close it under all circumstances.
struct wlr_xwm *xwm_create(struct wlr_xwayland *wlr_xwayland, int wm_fd);

void xwm_destroy(struct wlr_xwm *xwm);

void xwm_set_cursor(struct wlr_xwm *xwm, const uint8_t *pixels, uint32_t stride,
	uint32_t width, uint32_t height, int32_t hotspot_x, int32_t hotspot_y);

int xwm_handle_selection_event(struct wlr_xwm *xwm, xcb_generic_event_t *event);
int xwm_handle_selection_client_message(struct wlr_xwm *xwm,
	xcb_client_message_event_t *ev);
void xwm_seat_unlink_drag_handlers(struct wlr_xwm *xwm);

void xwm_set_seat(struct wlr_xwm *xwm, struct wlr_seat *seat);

char *xwm_get_atom_name(struct wlr_xwm *xwm, xcb_atom_t atom);
bool xwm_atoms_contains(struct wlr_xwm *xwm, xcb_atom_t *atoms,
	size_t num_atoms, enum atom_name needle);

xcb_void_cookie_t xwm_send_event_with_size(xcb_connection_t *c,
	uint8_t propagate, xcb_window_t destination,
	uint32_t event_mask, const void *event, uint32_t length);

void xwm_schedule_flush(struct wlr_xwm *xwm);

#endif
