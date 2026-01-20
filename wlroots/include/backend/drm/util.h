#ifndef BACKEND_DRM_UTIL_H
#define BACKEND_DRM_UTIL_H

#include <stdint.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

struct wlr_drm_connector;

// Calculates a more accurate refresh rate (mHz) than what mode itself provides
int32_t calculate_refresh_rate(const drmModeModeInfo *mode);
enum wlr_output_mode_aspect_ratio get_picture_aspect_ratio(const drmModeModeInfo *mode);
// Returns manufacturer based on pnp id
const char *get_pnp_manufacturer(const char code[static 3]);
// Populates the make/model/phys_{width,height} of output from the edid data
void parse_edid(struct wlr_drm_connector *conn, size_t len, const uint8_t *data);
const char *drm_connector_status_str(drmModeConnection status);
void generate_cvt_mode(drmModeModeInfo *mode, int hdisplay, int vdisplay,
	float vrefresh);

// Part of match_connectors_with_crtcs
#define UNMATCHED ((uint32_t)-1)

/**
 * Tries to match DRM connectors with DRM CRTCs.
 *
 * conns contains an array of bitmasks describing compatible CRTCs. For
 * instance bit 0 set in an connector element means that it's compatible with
 * CRTC 0.
 *
 * prev_crtcs contains connector indices each CRTC was previously matched with,
 * or UNMATCHED.
 *
 * new_crtcs is populated with the new connector indices.
 */
void match_connectors_with_crtcs(size_t num_conns,
	const uint32_t conns[static restrict num_conns],
	size_t num_crtcs, const uint32_t prev_crtcs[static restrict num_crtcs],
	uint32_t new_crtcs[static restrict num_crtcs]);

#endif
