#ifndef BACKEND_DRM_DRM_H
#define BACKEND_DRM_DRM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend/drm.h>
#include <wlr/backend/session.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/types/wlr_output_layer.h>
#include <xf86drmMode.h>
#include "backend/drm/iface.h"
#include "backend/drm/properties.h"
#include "backend/drm/renderer.h"

struct wlr_drm_viewport {
	struct wlr_fbox src_box;
	struct wlr_box dst_box;
};

struct wlr_drm_plane {
	uint32_t type;
	uint32_t id;

	/* Only initialized on multi-GPU setups */
	struct wlr_drm_surface mgpu_surf;

	/* Buffer submitted to the kernel, will be presented on next vblank */
	struct wlr_drm_fb *queued_fb;
	/* Buffer currently displayed on screen */
	struct wlr_drm_fb *current_fb;
	/* Viewport belonging to the last committed fb */
	struct wlr_drm_viewport viewport;

	struct wlr_drm_format_set formats;

	struct wlr_output_cursor_size *cursor_sizes;
	size_t cursor_sizes_len;

	struct wlr_drm_plane_props props;

	uint32_t initial_crtc_id;
	struct liftoff_plane *liftoff;
	struct liftoff_layer *liftoff_layer;
};

struct wlr_drm_layer {
	struct wlr_output_layer *wlr;
	struct liftoff_layer *liftoff;
	struct wlr_addon addon; // wlr_output_layer.addons
	struct wl_list link; // wlr_drm_crtc.layers

	/* Buffer to be submitted to the kernel on the next page-flip */
	struct wlr_drm_fb *pending_fb;
	/* Buffer submitted to the kernel, will be presented on next vblank */
	struct wlr_drm_fb *queued_fb;
	/* Buffer currently displayed on screen */
	struct wlr_drm_fb *current_fb;

	// One entry per wlr_drm_backend.planes
	bool *candidate_planes;
};

struct wlr_drm_crtc {
	uint32_t id;
	struct wlr_drm_lease *lease;
	struct liftoff_output *liftoff;
	struct liftoff_layer *liftoff_composition_layer;
	struct wl_list layers; // wlr_drm_layer.link

	// Atomic modesetting only
	bool own_mode_id;
	uint32_t mode_id;
	uint32_t gamma_lut;

	// Legacy only
	int legacy_gamma_size;

	struct wlr_drm_plane *primary;
	struct wlr_drm_plane *cursor;

	struct wlr_drm_crtc_props props;
};

struct wlr_drm_backend {
	struct wlr_backend backend;

	struct wlr_drm_backend *parent;
	const struct wlr_drm_interface *iface;
	bool addfb2_modifiers;

	int fd;
	char *name;
	struct wlr_device *dev;
	struct liftoff_device *liftoff;

	size_t num_crtcs;
	struct wlr_drm_crtc *crtcs;

	size_t num_planes;
	struct wlr_drm_plane *planes;

	struct wl_event_source *drm_event;

	struct wl_listener session_destroy;
	struct wl_listener session_active;
	struct wl_listener parent_destroy;
	struct wl_listener dev_change;
	struct wl_listener dev_remove;

	struct wl_list fbs; // wlr_drm_fb.link
	struct wl_list connectors; // wlr_drm_connector.link

	struct wl_list page_flips; // wlr_drm_page_flip.link

	/* Only initialized on multi-GPU setups */
	struct wlr_drm_renderer mgpu_renderer;

	struct wlr_session *session;

	uint64_t cursor_width, cursor_height;

	struct wlr_drm_format_set mgpu_formats;

	bool supports_tearing_page_flips;
};

struct wlr_drm_mode {
	struct wlr_output_mode wlr_mode;
	drmModeModeInfo drm_mode;
};

struct wlr_drm_device_state {
	bool modeset;
	bool nonblock;

	struct wlr_drm_connector_state *connectors;
	size_t connectors_len;
};

struct wlr_drm_connector_state {
	struct wlr_drm_connector *connector;
	const struct wlr_output_state *base;
	bool active;
	drmModeModeInfo mode;
	struct wlr_drm_fb *primary_fb;
	struct wlr_drm_viewport primary_viewport;
	struct wlr_drm_fb *cursor_fb;

	struct wlr_drm_syncobj_timeline *wait_timeline;
	uint64_t wait_point;

	// used by atomic
	uint32_t mode_id;
	uint32_t gamma_lut;
	uint32_t fb_damage_clips;
	int primary_in_fence_fd, out_fence_fd;
	bool vrr_enabled;
	uint32_t colorspace;
	uint32_t hdr_output_metadata;
};

/**
 * Per-page-flip tracking struct.
 *
 * We've asked for a state change in the kernel, and yet to receive a
 * notification for its completion. Currently, the kernel only has a queue
 * length of 1, and no way to modify your submissions after they're sent.
 *
 * However, we might have multiple in-flight page-flip events, for instance
 * when performing a non-blocking commit followed by a blocking commit. In
 * that case, conn will be set to NULL on the non-blocking commit to indicate
 * that it's been superseded.
 */
struct wlr_drm_page_flip {
	struct wl_list link; // wlr_drm_connector.page_flips
	struct wlr_drm_page_flip_connector *connectors;
	size_t connectors_len;
	// True if DRM_MODE_PAGE_FLIP_ASYNC was set
	bool async;
};

struct wlr_drm_page_flip_connector {
	uint32_t crtc_id;
	struct wlr_drm_connector *connector; // may be NULL
};

struct wlr_drm_connector {
	struct wlr_output output; // only valid if status != DISCONNECTED

	struct wlr_drm_backend *backend;
	char name[24];
	drmModeConnection status;
	uint32_t id;
	uint64_t max_bpc_bounds[2];
	struct wlr_drm_lease *lease;

	struct wlr_drm_crtc *crtc;
	uint32_t possible_crtcs;

	struct wlr_drm_connector_props props;

	bool cursor_enabled;
	int cursor_x, cursor_y;
	int cursor_width, cursor_height;
	int cursor_hotspot_x, cursor_hotspot_y;
	/* Buffer to be submitted to the kernel on the next page-flip */
	struct wlr_drm_fb *cursor_pending_fb;

	struct wl_list link; // wlr_drm_backend.connectors

	// Last committed page-flip
	struct wlr_drm_page_flip *pending_page_flip;

	// Atomic modesetting only
	uint32_t colorspace;
	uint32_t hdr_output_metadata;

	int32_t refresh;
};

struct wlr_drm_backend *get_drm_backend_from_backend(
	struct wlr_backend *wlr_backend);
bool check_drm_features(struct wlr_drm_backend *drm);
bool init_drm_resources(struct wlr_drm_backend *drm);
void finish_drm_resources(struct wlr_drm_backend *drm);
void scan_drm_connectors(struct wlr_drm_backend *state,
	struct wlr_device_hotplug_event *event);
void scan_drm_leases(struct wlr_drm_backend *drm);
bool commit_drm_device(struct wlr_drm_backend *drm,
	const struct wlr_backend_output_state *states, size_t states_len, bool test_only);
int handle_drm_event(int fd, uint32_t mask, void *data);
void destroy_drm_connector(struct wlr_drm_connector *conn);
bool drm_connector_is_cursor_visible(struct wlr_drm_connector *conn);
size_t drm_crtc_get_gamma_lut_size(struct wlr_drm_backend *drm,
	struct wlr_drm_crtc *crtc);
void drm_lease_destroy(struct wlr_drm_lease *lease);
void drm_page_flip_destroy(struct wlr_drm_page_flip *page_flip);

struct wlr_drm_layer *get_drm_layer(struct wlr_drm_backend *drm,
	struct wlr_output_layer *layer);

#if __STDC_VERSION__ >= 202311L

#define wlr_drm_conn_log(conn, verb, fmt, ...) \
	wlr_log(verb, "connector %s: " fmt, conn->name __VA_OPT__(,) __VA_ARGS__)
#define wlr_drm_conn_log_errno(conn, verb, fmt, ...) \
	wlr_log_errno(verb, "connector %s: " fmt, conn->name __VA_OPT__(,) __VA_ARGS__)

#else

#define wlr_drm_conn_log(conn, verb, fmt, ...) \
	wlr_log(verb, "connector %s: " fmt, conn->name, ##__VA_ARGS__)
#define wlr_drm_conn_log_errno(conn, verb, fmt, ...) \
	wlr_log_errno(verb, "connector %s: " fmt, conn->name, ##__VA_ARGS__)

#endif

#endif
