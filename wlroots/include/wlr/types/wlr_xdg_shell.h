/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_XDG_SHELL_H
#define WLR_TYPES_WLR_XDG_SHELL_H

#include <wayland-server-core.h>
#include <wayland-protocols/xdg-shell-enum.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/util/box.h>

struct wlr_xdg_shell {
	struct wl_global *global;
	uint32_t version;
	struct wl_list clients;
	struct wl_list popup_grabs;
	uint32_t ping_timeout;

	struct {
		struct wl_signal new_surface; // struct wlr_xdg_surface
		struct wl_signal new_toplevel; // struct wlr_xdg_toplevel
		struct wl_signal new_popup; // struct wlr_xdg_popup
		struct wl_signal destroy;
	} events;

	void *data;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_xdg_client {
	struct wlr_xdg_shell *shell;
	struct wl_resource *resource;
	struct wl_client *client;
	struct wl_list surfaces;

	struct wl_list link; // wlr_xdg_shell.clients

	uint32_t ping_serial;
	struct wl_event_source *ping_timer;
};

struct wlr_xdg_positioner_rules {
	struct wlr_box anchor_rect;
	enum xdg_positioner_anchor anchor;
	enum xdg_positioner_gravity gravity;
	enum xdg_positioner_constraint_adjustment constraint_adjustment;

	bool reactive;

	bool has_parent_configure_serial;
	uint32_t parent_configure_serial;

	struct {
		int32_t width, height;
	} size, parent_size;

	struct {
		int32_t x, y;
	} offset;
};

struct wlr_xdg_positioner {
	struct wl_resource *resource;
	struct wlr_xdg_positioner_rules rules;
};

struct wlr_xdg_popup_state {
	// Position of the popup relative to the upper left corner of
	// the window geometry of the parent surface
	struct wlr_box geometry;

	bool reactive;
};

enum wlr_xdg_popup_configure_field {
	WLR_XDG_POPUP_CONFIGURE_REPOSITION_TOKEN = 1 << 0,
};

struct wlr_xdg_popup_configure {
	uint32_t fields; // enum wlr_xdg_popup_configure_field
	struct wlr_box geometry;
	struct wlr_xdg_positioner_rules rules;
	uint32_t reposition_token;
};

struct wlr_xdg_popup {
	struct wlr_xdg_surface *base;
	struct wl_list link;

	struct wl_resource *resource;
	struct wlr_surface *parent;
	struct wlr_seat *seat;

	struct wlr_xdg_popup_configure scheduled;

	struct wlr_xdg_popup_state current, pending;

	struct {
		struct wl_signal destroy;

		struct wl_signal reposition;
	} events;

	struct wl_list grab_link; // wlr_xdg_popup_grab.popups

	struct {
		struct wlr_surface_synced synced;
	} WLR_PRIVATE;
};

// each seat gets a popup grab
struct wlr_xdg_popup_grab {
	struct wl_client *client;
	struct wlr_seat_pointer_grab pointer_grab;
	struct wlr_seat_keyboard_grab keyboard_grab;
	struct wlr_seat_touch_grab touch_grab;
	struct wlr_seat *seat;
	struct wl_list popups;
	struct wl_list link; // wlr_xdg_shell.popup_grabs

	struct {
		struct wl_listener seat_destroy;
	} WLR_PRIVATE;
};

enum wlr_xdg_surface_role {
	WLR_XDG_SURFACE_ROLE_NONE,
	WLR_XDG_SURFACE_ROLE_TOPLEVEL,
	WLR_XDG_SURFACE_ROLE_POPUP,
};

struct wlr_xdg_toplevel_state {
	bool maximized, fullscreen, resizing, activated, suspended;
	uint32_t tiled; // enum wlr_edges
	uint32_t constrained; // enum wlr_edges
	int32_t width, height;
	int32_t max_width, max_height;
	int32_t min_width, min_height;
};

enum wlr_xdg_toplevel_wm_capabilities {
	WLR_XDG_TOPLEVEL_WM_CAPABILITIES_WINDOW_MENU = 1 << 0,
	WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MAXIMIZE = 1 << 1,
	WLR_XDG_TOPLEVEL_WM_CAPABILITIES_FULLSCREEN = 1 << 2,
	WLR_XDG_TOPLEVEL_WM_CAPABILITIES_MINIMIZE = 1 << 3,
};

enum wlr_xdg_toplevel_configure_field {
	WLR_XDG_TOPLEVEL_CONFIGURE_BOUNDS = 1 << 0,
	WLR_XDG_TOPLEVEL_CONFIGURE_WM_CAPABILITIES = 1 << 1,
};

/**
 * State set in an toplevel configure sequence.
 */
struct wlr_xdg_toplevel_configure {
	// Bitmask of optional fields which are set
	uint32_t fields; // enum wlr_xdg_toplevel_configure_field

	// The following fields must always be set to reflect the current state
	bool maximized, fullscreen, resizing, activated, suspended;
	uint32_t tiled; // enum wlr_edges
	uint32_t constrained; // enum wlr_edges
	int32_t width, height;

	// Only for WLR_XDG_TOPLEVEL_CONFIGURE_BOUNDS
	struct {
		int32_t width, height;
	} bounds;
	// Only for WLR_XDG_TOPLEVEL_CONFIGURE_WM_CAPABILITIES
	uint32_t wm_capabilities; // enum wlr_xdg_toplevel_wm_capabilities
};

struct wlr_xdg_toplevel_requested {
	bool maximized, minimized, fullscreen;
	struct wlr_output *fullscreen_output;

	struct {
		struct wl_listener fullscreen_output_destroy;
	} WLR_PRIVATE;
};

struct wlr_xdg_toplevel {
	struct wl_resource *resource;
	struct wlr_xdg_surface *base;

	struct wlr_xdg_toplevel *parent;

	struct wlr_xdg_toplevel_state current, pending;

	// Properties to be sent to the client in the next configure event.
	struct wlr_xdg_toplevel_configure scheduled;

	// Properties that the client has requested. Intended to be checked
	// by the compositor on surface map and state change requests (such as
	// xdg_toplevel.set_fullscreen) and handled accordingly.
	struct wlr_xdg_toplevel_requested requested;

	char *title;
	char *app_id;

	struct {
		struct wl_signal destroy;

		// Note: as per xdg-shell protocol, the compositor has to
		// handle state requests by sending a configure event,
		// even if it didn't actually change the state. Therefore,
		// every compositor implementing xdg-shell support *must*
		// listen to these signals and schedule a configure event
		// immediately or at some time in the future; not doing so
		// is a protocol violation.
		struct wl_signal request_maximize;
		struct wl_signal request_fullscreen;

		struct wl_signal request_minimize;
		struct wl_signal request_move;
		struct wl_signal request_resize;
		struct wl_signal request_show_window_menu;
		struct wl_signal set_parent;
		struct wl_signal set_title;
		struct wl_signal set_app_id;
	} events;

	struct {
		struct wlr_surface_synced synced;

		struct wl_listener parent_unmap;
	} WLR_PRIVATE;
};

struct wlr_xdg_surface_configure {
	struct wlr_xdg_surface *surface;
	struct wl_list link; // wlr_xdg_surface.configure_list
	uint32_t serial;

	union {
		struct wlr_xdg_toplevel_configure *toplevel_configure;
		struct wlr_xdg_popup_configure *popup_configure;
	};
};

enum wlr_xdg_surface_state_field {
	WLR_XDG_SURFACE_STATE_WINDOW_GEOMETRY = 1 << 0,
};

struct wlr_xdg_surface_state {
	uint32_t committed; // enum wlr_xdg_surface_state_field

	struct wlr_box geometry;

	uint32_t configure_serial;
};

/**
 * An xdg-surface is a user interface element requiring management by the
 * compositor. An xdg-surface alone isn't useful, a role should be assigned to
 * it in order to map it.
 */
struct wlr_xdg_surface {
	struct wlr_xdg_client *client;
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wl_list link; // wlr_xdg_client.surfaces

	/**
	 * The lifetime-bound role of the xdg_surface. WLR_XDG_SURFACE_ROLE_NONE
	 * if the role was never set.
	 */
	enum wlr_xdg_surface_role role;
	/**
	 * The role object representing the role. NULL if the object was destroyed.
	 */
	struct wl_resource *role_resource;

	// NULL if the role resource is inert
	union {
		struct wlr_xdg_toplevel *toplevel;
		struct wlr_xdg_popup *popup;
	};

	struct wl_list popups; // wlr_xdg_popup.link

	bool configured;
	struct wl_event_source *configure_idle;
	uint32_t scheduled_serial;
	struct wl_list configure_list;

	struct wlr_xdg_surface_state current, pending;

	// Whether the surface is ready to receive configure events
	bool initialized;
	// Whether the latest commit is an initial commit
	bool initial_commit;

	struct wlr_box geometry;

	struct {
		struct wl_signal destroy;
		struct wl_signal ping_timeout;
		struct wl_signal new_popup;

		// for protocol extensions
		struct wl_signal configure; // struct wlr_xdg_surface_configure
		struct wl_signal ack_configure; // struct wlr_xdg_surface_configure
	} events;

	void *data;

	struct {
		struct wlr_surface_synced synced;

		struct wl_listener role_resource_destroy;
	} WLR_PRIVATE;
};

struct wlr_xdg_toplevel_move_event {
	struct wlr_xdg_toplevel *toplevel;
	struct wlr_seat_client *seat;
	uint32_t serial;
};

struct wlr_xdg_toplevel_resize_event {
	struct wlr_xdg_toplevel *toplevel;
	struct wlr_seat_client *seat;
	uint32_t serial;
	uint32_t edges;
};

struct wlr_xdg_toplevel_show_window_menu_event {
	struct wlr_xdg_toplevel *toplevel;
	struct wlr_seat_client *seat;
	uint32_t serial;
	int32_t x, y;
};

/**
 * Create the xdg_wm_base global with the specified version.
 */
struct wlr_xdg_shell *wlr_xdg_shell_create(struct wl_display *display,
	uint32_t version);

/** Get the corresponding struct wlr_xdg_surface from a resource.
 *
 * Aborts if the resource doesn't have the correct type. Returns NULL if the
 * resource is inert.
 */
struct wlr_xdg_surface *wlr_xdg_surface_from_resource(
		struct wl_resource *resource);

/** Get the corresponding struct wlr_xdg_popup from a resource.
 *
 * Aborts if the resource doesn't have the correct type. Returns NULL if the
 * resource is inert.
 */
struct wlr_xdg_popup *wlr_xdg_popup_from_resource(
		struct wl_resource *resource);

/** Get the corresponding struct wlr_xdg_toplevel from a resource.
 *
 * Aborts if the resource doesn't have the correct type. Returns NULL if the
 * resource is inert.
 */
struct wlr_xdg_toplevel *wlr_xdg_toplevel_from_resource(
		struct wl_resource *resource);

/** Get the corresponding struct wlr_xdg_positioner from a resource.
 *
 * Aborts if the resource doesn't have the correct type.
 */
struct wlr_xdg_positioner *wlr_xdg_positioner_from_resource(
		struct wl_resource *resource);

/**
 * Send a ping to the surface. If the surface does not respond in a reasonable
 * amount of time, the ping_timeout event will be emitted.
 */
void wlr_xdg_surface_ping(struct wlr_xdg_surface *surface);

/**
 * Configure the toplevel. Returns the associated configure serial.
 */
uint32_t wlr_xdg_toplevel_configure(struct wlr_xdg_toplevel *toplevel,
		const struct wlr_xdg_toplevel_configure *configure);

/**
 * Request that this toplevel surface be the given size. Returns the associated
 * configure serial.
 */
uint32_t wlr_xdg_toplevel_set_size(struct wlr_xdg_toplevel *toplevel,
		int32_t width, int32_t height);

/**
 * Request that this toplevel show itself in an activated or deactivated
 * state. Returns the associated configure serial.
 */
uint32_t wlr_xdg_toplevel_set_activated(struct wlr_xdg_toplevel *toplevel,
		bool activated);

/**
 * Request that this toplevel consider itself maximized or not
 * maximized. Returns the associated configure serial.
 */
uint32_t wlr_xdg_toplevel_set_maximized(struct wlr_xdg_toplevel *toplevel,
		bool maximized);

/**
 * Request that this toplevel consider itself fullscreen or not
 * fullscreen. Returns the associated configure serial.
 */
uint32_t wlr_xdg_toplevel_set_fullscreen(struct wlr_xdg_toplevel *toplevel,
		bool fullscreen);

/**
 * Request that this toplevel consider itself to be resizing or not
 * resizing. Returns the associated configure serial.
 */
uint32_t wlr_xdg_toplevel_set_resizing(struct wlr_xdg_toplevel *toplevel,
		bool resizing);

/**
 * Request that this toplevel consider itself in a tiled layout and some
 * edges are adjacent to another part of the tiling grid. `tiled_edges` is a
 * bitfield of enum wlr_edges. Returns the associated configure serial.
 */
uint32_t wlr_xdg_toplevel_set_tiled(struct wlr_xdg_toplevel *toplevel,
		uint32_t tiled_edges);

/**
 * Configure the recommended bounds for the client's window geometry size.
 * Returns the associated configure serial.
 */
uint32_t wlr_xdg_toplevel_set_bounds(struct wlr_xdg_toplevel *toplevel,
		int32_t width, int32_t height);

/**
 * Configure the window manager capabilities for this toplevel. `caps` is a
 * bitfield of `enum wlr_xdg_toplevel_wm_capabilities`. Returns the associated
 * configure serial.
 */
uint32_t wlr_xdg_toplevel_set_wm_capabilities(struct wlr_xdg_toplevel *toplevel,
		uint32_t caps);

/**
 * Request that this toplevel consider itself suspended or not
 * suspended. Returns the associated configure serial.
 */
uint32_t wlr_xdg_toplevel_set_suspended(struct wlr_xdg_toplevel *toplevel,
		bool suspended);

/**
 * Request that this toplevel consider itself constrained and doesn't attempt to
 * resize from some edges. `constrained_edges` is a bitfield of enum wlr_edges.
 * Returns the associated configure serial.
 */
uint32_t wlr_xdg_toplevel_set_constrained(struct wlr_xdg_toplevel *toplevel,
		uint32_t constrained_edges);

/**
 * Request that this toplevel closes.
 */
void wlr_xdg_toplevel_send_close(struct wlr_xdg_toplevel *toplevel);

/**
 * Sets the parent of this toplevel. Parent can be NULL.
 *
 * Returns true on success, false if setting the parent would create a loop.
 */
bool wlr_xdg_toplevel_set_parent(struct wlr_xdg_toplevel *toplevel,
	struct wlr_xdg_toplevel *parent);

/**
 * Notify the client that the popup has been dismissed and destroy the
 * struct wlr_xdg_popup, rendering the resource inert.
 */
void wlr_xdg_popup_destroy(struct wlr_xdg_popup *popup);

/**
 * Get the position for this popup in the surface parent's coordinate system.
 */
void wlr_xdg_popup_get_position(struct wlr_xdg_popup *popup,
		double *popup_sx, double *popup_sy);

/**
 * Returns true if a positioner is complete.
 */
bool wlr_xdg_positioner_is_complete(struct wlr_xdg_positioner *positioner);

/**
 * Get the geometry based on positioner rules.
 */
void wlr_xdg_positioner_rules_get_geometry(
		const struct wlr_xdg_positioner_rules *rules, struct wlr_box *box);

/**
 * Unconstrain the box from the constraint area according to positioner rules.
 */
void wlr_xdg_positioner_rules_unconstrain_box(
		const struct wlr_xdg_positioner_rules *rules,
		const struct wlr_box *constraint, struct wlr_box *box);

/**
 * Convert the given coordinates in the popup coordinate system to the toplevel
 * surface coordinate system.
 */
void wlr_xdg_popup_get_toplevel_coords(struct wlr_xdg_popup *popup,
		int popup_sx, int popup_sy, int *toplevel_sx, int *toplevel_sy);

/**
 * Set the geometry of this popup to unconstrain it according to its
 * xdg-positioner rules. The box should be in the popup's root toplevel parent
 * surface coordinate system.
 */
void wlr_xdg_popup_unconstrain_from_box(struct wlr_xdg_popup *popup,
		const struct wlr_box *toplevel_space_box);

/**
 * Find a surface within this xdg-surface tree at the given surface-local
 * coordinates. Returns the surface and coordinates in the leaf surface
 * coordinate system or NULL if no surface is found at that location.
 */
struct wlr_surface *wlr_xdg_surface_surface_at(
		struct wlr_xdg_surface *surface, double sx, double sy,
		double *sub_x, double *sub_y);

/**
 * Find a surface within this xdg-surface's popup tree at the given
 * surface-local coordinates. Returns the surface and coordinates in the leaf
 * surface coordinate system or NULL if no surface is found at that location.
 */
struct wlr_surface *wlr_xdg_surface_popup_surface_at(
		struct wlr_xdg_surface *surface, double sx, double sy,
		double *sub_x, double *sub_y);

/**
 * Get a struct wlr_xdg_surface from a struct wlr_surface.
 *
 * Returns NULL if the surface doesn't have the xdg_surface role or
 * if the xdg_surface has been destroyed.
 */
struct wlr_xdg_surface *wlr_xdg_surface_try_from_wlr_surface(struct wlr_surface *surface);

/**
 * Get a struct wlr_xdg_toplevel from a struct wlr_surface.
 *
 * Returns NULL if the surface doesn't have the xdg_surface role, the
 * xdg_surface is not a toplevel, or the xdg_surface/xdg_toplevel objects have
 * been destroyed.
 */
struct wlr_xdg_toplevel *wlr_xdg_toplevel_try_from_wlr_surface(struct wlr_surface *surface);

/**
 * Get a struct wlr_xdg_popup from a struct wlr_surface.
 *
 * Returns NULL if the surface doesn't have the xdg_surface role, the
 * xdg_surface is not a popup, or the xdg_surface/xdg_popup objects have
 * been destroyed.
 */
struct wlr_xdg_popup *wlr_xdg_popup_try_from_wlr_surface(struct wlr_surface *surface);

/**
 * Call `iterator` on each mapped surface and popup in the xdg-surface tree
 * (whether or not this xdg-surface is mapped), with the surface's position
 * relative to the root xdg-surface. The function is called from root to leaves
 * (in rendering order).
 */
void wlr_xdg_surface_for_each_surface(struct wlr_xdg_surface *surface,
		wlr_surface_iterator_func_t iterator, void *user_data);

/**
 * Call `iterator` on each mapped popup's surface and popup's subsurface in the
 * xdg-surface tree (whether or not this xdg-surface is mapped), with the
 * surfaces's position relative to the root xdg-surface. The function is called
 * from root to leaves (in rendering order).
 */
void wlr_xdg_surface_for_each_popup_surface(struct wlr_xdg_surface *surface,
		wlr_surface_iterator_func_t iterator, void *user_data);

/**
 * Schedule a surface configuration. This should only be called by protocols
 * extending the shell.
 */
uint32_t wlr_xdg_surface_schedule_configure(struct wlr_xdg_surface *surface);

#endif
