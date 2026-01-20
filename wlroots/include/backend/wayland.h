#ifndef BACKEND_WAYLAND_H
#define BACKEND_WAYLAND_H

#include <stdbool.h>

#include <wayland-client-protocol.h>
#include <wayland-server-core.h>

#include <wlr/backend/wayland.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_tablet_pad.h>
#include <wlr/types/wlr_tablet_tool.h>
#include <wlr/types/wlr_touch.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/render/drm_syncobj.h>

struct wlr_wl_backend {
	struct wlr_backend backend;

	/* local state */
	bool started;
	struct wl_event_loop *event_loop;
	struct wl_event_queue *busy_loop_queue;
	struct wl_list outputs;
	int drm_fd;
	struct wl_list buffers; // wlr_wl_buffer.link
	size_t requested_outputs;
	struct wl_listener event_loop_destroy;
	char *activation_token;

	/* remote state */
	struct wl_display *remote_display;
	bool own_remote_display;
	struct wl_event_source *remote_display_src;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct xdg_wm_base *xdg_wm_base;
	struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1;
	struct zwp_pointer_gestures_v1 *zwp_pointer_gestures_v1;
	struct wp_presentation *presentation;
	struct wl_shm *shm;
	struct zwp_linux_dmabuf_v1 *zwp_linux_dmabuf_v1;
	struct wp_linux_drm_syncobj_manager_v1 *drm_syncobj_manager_v1;
	struct wl_list drm_syncobj_timelines; // wlr_wl_drm_syncobj_timeline.link
	struct zwp_relative_pointer_manager_v1 *zwp_relative_pointer_manager_v1;
	struct wl_list seats; // wlr_wl_seat.link
	struct zwp_tablet_manager_v2 *tablet_manager;
	struct wlr_drm_format_set shm_formats;
	struct wlr_drm_format_set linux_dmabuf_v1_formats;
	struct wl_drm *legacy_drm;
	struct xdg_activation_v1 *activation_v1;
	struct wl_subcompositor *subcompositor;
	struct wp_viewporter *viewporter;
	char *drm_render_name;
};

struct wlr_wl_buffer {
	struct wlr_buffer *buffer;
	struct wl_buffer *wl_buffer;
	bool released;
	struct wl_list link; // wlr_wl_backend.buffers
	struct wl_listener buffer_destroy;

	bool has_drm_syncobj_waiter;
	struct wlr_drm_syncobj_timeline_waiter drm_syncobj_waiter;

	struct wlr_drm_syncobj_timeline *fallback_signal_timeline;
	uint64_t fallback_signal_point;
};

struct wlr_wl_drm_syncobj_timeline {
	struct wlr_drm_syncobj_timeline *base;
	struct wlr_addon addon;
	struct wl_list link; // wlr_wl_backend.drm_syncobj_timelines
	struct wp_linux_drm_syncobj_timeline_v1 *wl;
};

struct wlr_wl_presentation_feedback {
	struct wlr_wl_output *output;
	struct wl_list link;
	struct wp_presentation_feedback *feedback;
	uint32_t commit_seq;
};

struct wlr_wl_output_layer {
	struct wlr_addon addon;

	struct wl_surface *surface;
	struct wl_subsurface *subsurface;
	struct wp_viewport *viewport;
	bool mapped;
};

struct wlr_wl_output {
	struct wlr_output wlr_output;

	struct wlr_wl_backend *backend;
	struct wl_list link;

	struct wl_surface *surface;
	bool own_surface;
	struct wl_callback *frame_callback;
	struct xdg_surface *xdg_surface;
	struct xdg_toplevel *xdg_toplevel;
	struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1;
	struct wp_linux_drm_syncobj_surface_v1 *drm_syncobj_surface_v1;
	struct wl_list presentation_feedbacks;

	char *title;
	char *app_id;

	// 0 if not requested
	int32_t requested_width, requested_height;

	uint32_t configure_serial;
	bool has_configure_serial;
	bool configured;

	bool initialized;

	// If not NULL, the host compositor hasn't acknowledged the unmapping yet;
	// ignore all configure events
	struct wl_callback *unmap_callback;

	uint32_t enter_serial;

	struct {
		struct wlr_wl_pointer *pointer;
		struct wl_surface *surface;
		int32_t hotspot_x, hotspot_y;
	} cursor;
};

struct wlr_wl_pointer {
	struct wlr_pointer wlr_pointer;

	struct wlr_wl_seat *seat;
	struct wlr_wl_output *output;

	enum wl_pointer_axis_source axis_source;
	int32_t axis_discrete;
	uint32_t fingers; // trackpad gesture
	enum wl_pointer_axis_relative_direction axis_relative_direction;

	struct wl_listener output_destroy;

	struct wl_list link;
};

struct wlr_wl_touch_points {
	int32_t ids[64];
	size_t len;
};

struct wlr_wl_seat {
	char *name;
	struct wl_seat *wl_seat;
	uint32_t global_name;

	struct wlr_wl_backend *backend;

	struct wl_keyboard *wl_keyboard;
	struct wlr_keyboard wlr_keyboard;

	struct wl_pointer *wl_pointer;
	struct wlr_wl_pointer *active_pointer;
	struct wl_list pointers; // wlr_wl_pointer.link

	struct zwp_pointer_gesture_swipe_v1 *gesture_swipe;
	struct zwp_pointer_gesture_pinch_v1 *gesture_pinch;
	struct zwp_pointer_gesture_hold_v1 *gesture_hold;
	struct zwp_relative_pointer_v1 *relative_pointer;

	struct wl_touch *wl_touch;
	struct wlr_touch wlr_touch;
	struct wlr_wl_touch_points touch_points;

	struct zwp_tablet_seat_v2 *zwp_tablet_seat_v2;
	struct zwp_tablet_v2 *zwp_tablet_v2;
	struct wlr_tablet wlr_tablet;
	struct zwp_tablet_tool_v2 *zwp_tablet_tool_v2;
	struct wlr_tablet_tool wlr_tablet_tool;
	struct zwp_tablet_pad_v2 *zwp_tablet_pad_v2;
	struct wlr_tablet_pad wlr_tablet_pad;

	struct wl_list link; // wlr_wl_backend.seats
};

struct wlr_wl_backend *get_wl_backend_from_backend(struct wlr_backend *backend);
struct wlr_wl_output *get_wl_output_from_surface(struct wlr_wl_backend *wl,
	struct wl_surface *surface);
void update_wl_output_cursor(struct wlr_wl_output *output);

void init_seat_keyboard(struct wlr_wl_seat *seat);

void init_seat_pointer(struct wlr_wl_seat *seat);
void finish_seat_pointer(struct wlr_wl_seat *seat);
void create_pointer(struct wlr_wl_seat *seat, struct wlr_wl_output *output);

void init_seat_touch(struct wlr_wl_seat *seat);

void init_seat_tablet(struct wlr_wl_seat *seat);
void finish_seat_tablet(struct wlr_wl_seat *seat);

bool create_wl_seat(struct wl_seat *wl_seat, struct wlr_wl_backend *wl,
	uint32_t global_name);
void destroy_wl_seat(struct wlr_wl_seat *seat);
void destroy_wl_buffer(struct wlr_wl_buffer *buffer);
void destroy_wl_drm_syncobj_timeline(struct wlr_wl_drm_syncobj_timeline *timeline);

extern const struct wlr_pointer_impl wl_pointer_impl;
extern const struct wlr_tablet_pad_impl wl_tablet_pad_impl;
extern const struct wlr_tablet_impl wl_tablet_impl;

#endif
