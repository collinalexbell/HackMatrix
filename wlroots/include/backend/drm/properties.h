#ifndef BACKEND_DRM_PROPERTIES_H
#define BACKEND_DRM_PROPERTIES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * These types contain the property ids for several DRM objects.
 * For more details, see:
 * https://dri.freedesktop.org/docs/drm/gpu/drm-kms.html#kms-properties
 */

struct wlr_drm_connector_props {
	uint32_t edid;
	uint32_t dpms;
	uint32_t link_status; // not guaranteed to exist
	uint32_t path;
	uint32_t vrr_capable; // not guaranteed to exist
	uint32_t subconnector; // not guaranteed to exist
	uint32_t non_desktop;
	uint32_t panel_orientation; // not guaranteed to exist
	uint32_t content_type; // not guaranteed to exist
	uint32_t max_bpc; // not guaranteed to exist

	// atomic-modesetting only

	uint32_t crtc_id;
	uint32_t colorspace;
	uint32_t hdr_output_metadata;
};

struct wlr_drm_crtc_props {
	// Neither of these are guaranteed to exist
	uint32_t vrr_enabled;
	uint32_t gamma_lut;
	uint32_t gamma_lut_size;

	// atomic-modesetting only

	uint32_t active;
	uint32_t mode_id;
	uint32_t out_fence_ptr;
};

struct wlr_drm_plane_props {
	uint32_t type;
	uint32_t rotation; // Not guaranteed to exist
	uint32_t in_formats; // Not guaranteed to exist
	uint32_t size_hints; // Not guaranteed to exist

	// atomic-modesetting only

	uint32_t src_x;
	uint32_t src_y;
	uint32_t src_w;
	uint32_t src_h;
	uint32_t crtc_x;
	uint32_t crtc_y;
	uint32_t crtc_w;
	uint32_t crtc_h;
	uint32_t fb_id;
	uint32_t crtc_id;
	uint32_t fb_damage_clips;
	uint32_t hotspot_x;
	uint32_t hotspot_y;
	uint32_t in_fence_fd;
};

bool get_drm_connector_props(int fd, uint32_t id,
	struct wlr_drm_connector_props *out);
bool get_drm_crtc_props(int fd, uint32_t id, struct wlr_drm_crtc_props *out);
bool get_drm_plane_props(int fd, uint32_t id, struct wlr_drm_plane_props *out);

bool get_drm_prop(int fd, uint32_t obj, uint32_t prop, uint64_t *ret);
void *get_drm_prop_blob(int fd, uint32_t obj, uint32_t prop, size_t *ret_len);
char *get_drm_prop_enum(int fd, uint32_t obj, uint32_t prop);

bool introspect_drm_prop_range(int fd, uint32_t prop_id,
	uint64_t *min, uint64_t *max);

#endif
