#include <drm_fourcc.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <xf86drmMode.h>
#include "backend/drm/drm.h"
#include "backend/drm/fb.h"
#include "backend/drm/iface.h"
#include "backend/drm/util.h"
#include "render/color.h"

static char *atomic_commit_flags_str(uint32_t flags) {
	const char *const l[] = {
		(flags & DRM_MODE_PAGE_FLIP_EVENT) ? "PAGE_FLIP_EVENT" : NULL,
		(flags & DRM_MODE_PAGE_FLIP_ASYNC) ? "PAGE_FLIP_ASYNC" : NULL,
		(flags & DRM_MODE_ATOMIC_TEST_ONLY) ? "ATOMIC_TEST_ONLY" : NULL,
		(flags & DRM_MODE_ATOMIC_NONBLOCK) ? "ATOMIC_NONBLOCK" : NULL,
		(flags & DRM_MODE_ATOMIC_ALLOW_MODESET) ? "ATOMIC_ALLOW_MODESET" : NULL,
	};

	char *buf = NULL;
	size_t size = 0;
	FILE *f = open_memstream(&buf, &size);
	if (f == NULL) {
		return NULL;
	}

	for (size_t i = 0; i < sizeof(l) / sizeof(l[0]); i++) {
		if (l[i] == NULL) {
			continue;
		}
		if (ftell(f) > 0) {
			fprintf(f, " | ");
		}
		fprintf(f, "%s", l[i]);
	}

	if (ftell(f) == 0) {
		fprintf(f, "none");
	}

	fclose(f);

	return buf;
}

struct atomic {
	drmModeAtomicReq *req;
	bool failed;
};

static void atomic_begin(struct atomic *atom) {
	*atom = (struct atomic){0};

	atom->req = drmModeAtomicAlloc();
	if (!atom->req) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		atom->failed = true;
		return;
	}
}

static bool atomic_commit(struct atomic *atom, struct wlr_drm_backend *drm,
		const struct wlr_drm_device_state *state,
		struct wlr_drm_page_flip *page_flip, uint32_t flags) {
	if (atom->failed) {
		return false;
	}

	int ret = drmModeAtomicCommit(drm->fd, atom->req, flags, page_flip);
	if (ret != 0) {
		enum wlr_log_importance log_level = WLR_ERROR;
		if (flags & DRM_MODE_ATOMIC_TEST_ONLY) {
			log_level = WLR_DEBUG;
		}

		if (state->connectors_len == 1) {
			struct wlr_drm_connector *conn = state->connectors[0].connector;
			wlr_drm_conn_log_errno(conn, log_level, "Atomic commit failed");
		} else {
			wlr_log_errno(log_level, "Atomic commit failed");
		}
		char *flags_str = atomic_commit_flags_str(flags);
		wlr_log(WLR_DEBUG, "(Atomic commit flags: %s)",
			flags_str ? flags_str : "<error>");
		free(flags_str);
		return false;
	}

	return true;
}

static void atomic_finish(struct atomic *atom) {
	drmModeAtomicFree(atom->req);
}

static void atomic_add(struct atomic *atom, uint32_t id, uint32_t prop, uint64_t val) {
	if (!atom->failed && drmModeAtomicAddProperty(atom->req, id, prop, val) < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to add atomic DRM property");
		atom->failed = true;
	}
}

static bool create_mode_blob(struct wlr_drm_connector *conn,
		const struct wlr_drm_connector_state *state, uint32_t *blob_id) {
	if (!state->active) {
		*blob_id = 0;
		return true;
	}

	if (drmModeCreatePropertyBlob(conn->backend->fd, &state->mode,
			sizeof(drmModeModeInfo), blob_id)) {
		wlr_log_errno(WLR_ERROR, "Unable to create mode property blob");
		return false;
	}

	return true;
}

static bool create_gamma_lut_blob(struct wlr_drm_backend *drm,
		size_t size, const uint16_t *lut, uint32_t *blob_id) {
	if (size == 0) {
		*blob_id = 0;
		return true;
	}

	struct drm_color_lut *gamma = malloc(size * sizeof(*gamma));
	if (gamma == NULL) {
		wlr_log(WLR_ERROR, "Failed to allocate gamma table");
		return false;
	}

	const uint16_t *r = lut;
	const uint16_t *g = lut + size;
	const uint16_t *b = lut + 2 * size;
	for (size_t i = 0; i < size; i++) {
		gamma[i].red = r[i];
		gamma[i].green = g[i];
		gamma[i].blue = b[i];
	}

	if (drmModeCreatePropertyBlob(drm->fd, gamma,
			size * sizeof(*gamma), blob_id) != 0) {
		wlr_log_errno(WLR_ERROR, "Unable to create gamma LUT property blob");
		free(gamma);
		return false;
	}
	free(gamma);

	return true;
}

bool create_fb_damage_clips_blob(struct wlr_drm_backend *drm,
		int width, int height, const pixman_region32_t *damage, uint32_t *blob_id) {
	pixman_region32_t clipped;
	pixman_region32_init(&clipped);
	pixman_region32_intersect_rect(&clipped, damage, 0, 0, width, height);

	int rects_len;
	const pixman_box32_t *rects = pixman_region32_rectangles(&clipped, &rects_len);

	int ret;
	if (rects_len > 0) {
		ret = drmModeCreatePropertyBlob(drm->fd, rects, sizeof(*rects) * rects_len, blob_id);
	} else {
		ret = 0;
		*blob_id = 0;
	}
	pixman_region32_fini(&clipped);
	if (ret != 0) {
		wlr_log_errno(WLR_ERROR, "Failed to create FB_DAMAGE_CLIPS property blob");
		return false;
	}

	return true;
}

static uint8_t convert_cta861_eotf(enum wlr_color_transfer_function tf) {
	switch (tf) {
	case WLR_COLOR_TRANSFER_FUNCTION_SRGB:
		abort(); // unsupported
	case WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ:
		return 2;
	case WLR_COLOR_TRANSFER_FUNCTION_EXT_LINEAR:
		abort(); // unsupported
	case WLR_COLOR_TRANSFER_FUNCTION_GAMMA22:
		abort(); // unsupported
	case WLR_COLOR_TRANSFER_FUNCTION_BT1886:
		abort(); // unsupported
	}
	abort(); // unreachable
}

static uint16_t convert_cta861_color_coord(double v) {
	if (v < 0) {
		v = 0;
	}
	if (v > 1) {
		v = 1;
	}
	return (uint16_t)round(v * 50000);
}

static bool create_hdr_output_metadata_blob(struct wlr_drm_backend *drm,
		const struct wlr_output_image_description *img_desc, uint32_t *blob_id) {
	if (img_desc == NULL) {
		*blob_id = 0;
		return true;
	}

	struct hdr_output_metadata metadata = {
		.metadata_type = 0,
		.hdmi_metadata_type1 = {
			.eotf = convert_cta861_eotf(img_desc->transfer_function),
			.metadata_type = 0,
			.display_primaries = {
				{
					.x = convert_cta861_color_coord(img_desc->mastering_display_primaries.red.x),
					.y = convert_cta861_color_coord(img_desc->mastering_display_primaries.red.y),
				},
				{
					.x = convert_cta861_color_coord(img_desc->mastering_display_primaries.green.x),
					.y = convert_cta861_color_coord(img_desc->mastering_display_primaries.green.y),
				},
				{
					.x = convert_cta861_color_coord(img_desc->mastering_display_primaries.blue.x),
					.y = convert_cta861_color_coord(img_desc->mastering_display_primaries.blue.y),
				},
			},
			.white_point = {
				.x = convert_cta861_color_coord(img_desc->mastering_display_primaries.white.x),
				.y = convert_cta861_color_coord(img_desc->mastering_display_primaries.white.y),
			},
			.max_display_mastering_luminance = img_desc->mastering_luminance.max,
			.min_display_mastering_luminance = img_desc->mastering_luminance.min * 0.0001,
			.max_cll = img_desc->max_cll,
			.max_fall = img_desc->max_fall,
		},
	};
	if (drmModeCreatePropertyBlob(drm->fd, &metadata, sizeof(metadata), blob_id) != 0) {
		wlr_log_errno(WLR_ERROR, "Failed to create HDR_OUTPUT_METADATA property");
		return false;
	}
	return true;
}

static uint64_t convert_primaries_to_colorspace(uint32_t primaries) {
	switch (primaries) {
	case 0:
		return 0; // Default
	case WLR_COLOR_NAMED_PRIMARIES_BT2020:
		return 9; // BT2020_RGB
	}
	abort(); // unreachable
}

static uint64_t max_bpc_for_format(uint32_t format) {
	switch (format) {
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_ABGR2101010:
		return 10;
	case DRM_FORMAT_XBGR16161616F:
	case DRM_FORMAT_ABGR16161616F:
	case DRM_FORMAT_XBGR16161616:
	case DRM_FORMAT_ABGR16161616:
		return 16;
	default:
		return 8;
	}
}

static uint64_t pick_max_bpc(struct wlr_drm_connector *conn, struct wlr_drm_fb *fb) {
	uint32_t format = DRM_FORMAT_INVALID;
	struct wlr_dmabuf_attributes attribs = {0};
	if (wlr_buffer_get_dmabuf(fb->wlr_buf, &attribs)) {
		format = attribs.format;
	}

	uint64_t target_bpc = max_bpc_for_format(format);
	if (target_bpc < conn->max_bpc_bounds[0]) {
		target_bpc = conn->max_bpc_bounds[0];
	}
	if (target_bpc > conn->max_bpc_bounds[1]) {
		target_bpc = conn->max_bpc_bounds[1];
	}
	return target_bpc;
}

static void destroy_blob(struct wlr_drm_backend *drm, uint32_t id) {
	if (id == 0) {
		return;
	}
	if (drmModeDestroyPropertyBlob(drm->fd, id) != 0) {
		wlr_log_errno(WLR_ERROR, "Failed to destroy blob");
	}
}

static void commit_blob(struct wlr_drm_backend *drm,
		uint32_t *current, uint32_t next) {
	if (*current == next) {
		return;
	}
	destroy_blob(drm, *current);
	*current = next;
}

static void rollback_blob(struct wlr_drm_backend *drm,
		uint32_t *current, uint32_t next) {
	if (*current == next) {
		return;
	}
	destroy_blob(drm, next);
}

bool drm_atomic_connector_prepare(struct wlr_drm_connector_state *state, bool modeset) {
	struct wlr_drm_connector *conn = state->connector;
	struct wlr_drm_backend *drm = conn->backend;
	struct wlr_output *output = &conn->output;
	struct wlr_drm_crtc *crtc = conn->crtc;

	uint32_t mode_id = crtc->mode_id;
	if (modeset) {
		if (!create_mode_blob(conn, state, &mode_id)) {
			return false;
		}
	}

	uint32_t gamma_lut = crtc->gamma_lut;
	if (state->base->committed & WLR_OUTPUT_STATE_COLOR_TRANSFORM) {
		size_t dim = 0;
		uint16_t *lut = NULL;
		if (state->base->color_transform != NULL) {
			struct wlr_color_transform_lut_3x1d *tr =
				color_transform_lut_3x1d_from_base(state->base->color_transform);
			dim = tr->dim;
			lut = tr->lut_3x1d;
		}

		// Fallback to legacy gamma interface when gamma properties are not
		// available (can happen on older Intel GPUs that support gamma but not
		// degamma).
		if (crtc->props.gamma_lut == 0) {
			if (!drm_legacy_crtc_set_gamma(drm, crtc, dim, lut)) {
				return false;
			}
		} else {
			if (!create_gamma_lut_blob(drm, dim, lut, &gamma_lut)) {
				return false;
			}
		}
	}

	uint32_t fb_damage_clips = 0;
	if ((state->base->committed & WLR_OUTPUT_STATE_DAMAGE) &&
			crtc->primary->props.fb_damage_clips != 0) {
		create_fb_damage_clips_blob(drm, state->primary_fb->wlr_buf->width,
			state->primary_fb->wlr_buf->height, &state->base->damage, &fb_damage_clips);
	}

	int in_fence_fd = -1;
	if (state->wait_timeline != NULL) {
		in_fence_fd = wlr_drm_syncobj_timeline_export_sync_file(state->wait_timeline,
			state->wait_point);
		if (in_fence_fd < 0) {
			return false;
		}
	}

	bool prev_vrr_enabled =
		output->adaptive_sync_status == WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED;
	bool vrr_enabled = prev_vrr_enabled;
	if ((state->base->committed & WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED)) {
		if (state->base->adaptive_sync_enabled && !output->adaptive_sync_supported) {
			return false;
		}
		vrr_enabled = state->base->adaptive_sync_enabled;
	}

	uint32_t colorspace = conn->colorspace;
	if (state->base->committed & WLR_OUTPUT_STATE_IMAGE_DESCRIPTION) {
		colorspace = convert_primaries_to_colorspace(
			state->base->image_description ? state->base->image_description->primaries : 0);
	}

	uint32_t hdr_output_metadata = conn->hdr_output_metadata;
	if ((state->base->committed & WLR_OUTPUT_STATE_IMAGE_DESCRIPTION) &&
			!create_hdr_output_metadata_blob(drm, state->base->image_description, &hdr_output_metadata)) {
		return false;
	}

	state->mode_id = mode_id;
	state->gamma_lut = gamma_lut;
	state->fb_damage_clips = fb_damage_clips;
	state->primary_in_fence_fd = in_fence_fd;
	state->vrr_enabled = vrr_enabled;
	state->colorspace = colorspace;
	state->hdr_output_metadata = hdr_output_metadata;
	return true;
}

void drm_atomic_connector_apply_commit(struct wlr_drm_connector_state *state) {
	struct wlr_drm_connector *conn = state->connector;
	struct wlr_drm_crtc *crtc = conn->crtc;
	struct wlr_drm_backend *drm = conn->backend;

	if (!crtc->own_mode_id) {
		crtc->mode_id = 0; // don't try to delete previous master's blobs
	}
	crtc->own_mode_id = true;
	commit_blob(drm, &crtc->mode_id, state->mode_id);
	commit_blob(drm, &crtc->gamma_lut, state->gamma_lut);
	commit_blob(drm, &conn->hdr_output_metadata, state->hdr_output_metadata);

	conn->output.adaptive_sync_status = state->vrr_enabled ?
		WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED : WLR_OUTPUT_ADAPTIVE_SYNC_DISABLED;

	destroy_blob(drm, state->fb_damage_clips);
	if (state->primary_in_fence_fd >= 0) {
		close(state->primary_in_fence_fd);
	}
	if (state->out_fence_fd >= 0) {
		// TODO: error handling
		wlr_drm_syncobj_timeline_import_sync_file(state->base->signal_timeline,
			state->base->signal_point, state->out_fence_fd);
		close(state->out_fence_fd);
	}

	conn->colorspace = state->colorspace;
}

void drm_atomic_connector_rollback_commit(struct wlr_drm_connector_state *state) {
	struct wlr_drm_connector *conn = state->connector;
	struct wlr_drm_crtc *crtc = conn->crtc;
	struct wlr_drm_backend *drm = conn->backend;

	rollback_blob(drm, &crtc->mode_id, state->mode_id);
	rollback_blob(drm, &crtc->gamma_lut, state->gamma_lut);
	rollback_blob(drm, &conn->hdr_output_metadata, state->hdr_output_metadata);

	destroy_blob(drm, state->fb_damage_clips);
	if (state->primary_in_fence_fd >= 0) {
		close(state->primary_in_fence_fd);
	}
	if (state->out_fence_fd >= 0) {
		close(state->out_fence_fd);
	}
}

static void plane_disable(struct atomic *atom, struct wlr_drm_plane *plane) {
	uint32_t id = plane->id;
	const struct wlr_drm_plane_props *props = &plane->props;
	atomic_add(atom, id, props->fb_id, 0);
	atomic_add(atom, id, props->crtc_id, 0);
}

static void set_plane_props(struct atomic *atom, struct wlr_drm_backend *drm,
		struct wlr_drm_plane *plane, struct wlr_drm_fb *fb, uint32_t crtc_id,
		const struct wlr_box *dst_box,
		const struct wlr_fbox *src_box) {
	uint32_t id = plane->id;
	const struct wlr_drm_plane_props *props = &plane->props;

	if (fb == NULL) {
		wlr_log(WLR_ERROR, "Failed to acquire FB for plane %"PRIu32, plane->id);
		atom->failed = true;
		return;
	}

	// The src_* properties are in 16.16 fixed point
	atomic_add(atom, id, props->src_x, src_box->x * (1 << 16));
	atomic_add(atom, id, props->src_y, src_box->y * (1 << 16));
	atomic_add(atom, id, props->src_w, src_box->width * (1 << 16));
	atomic_add(atom, id, props->src_h, src_box->height * (1 << 16));
	atomic_add(atom, id, props->fb_id, fb->id);
	atomic_add(atom, id, props->crtc_id, crtc_id);
	atomic_add(atom, id, props->crtc_x, dst_box->x);
	atomic_add(atom, id, props->crtc_y, dst_box->y);
	atomic_add(atom, id, props->crtc_w, dst_box->width);
	atomic_add(atom, id, props->crtc_h, dst_box->height);
}

static bool supports_cursor_hotspots(const struct wlr_drm_plane *plane) {
	return plane->props.hotspot_x && plane->props.hotspot_y;
}

static void set_plane_in_fence_fd(struct atomic *atom,
		struct wlr_drm_plane *plane, int sync_file_fd) {
	if (!plane->props.in_fence_fd) {
		wlr_log(WLR_ERROR, "Plane %"PRIu32 " is missing the IN_FENCE_FD property",
			plane->id);
		atom->failed = true;
		return;
	}

	atomic_add(atom, plane->id, plane->props.in_fence_fd, sync_file_fd);
}

static void set_crtc_out_fence_ptr(struct atomic *atom, struct wlr_drm_crtc *crtc,
		int *fd_ptr) {
	if (!crtc->props.out_fence_ptr) {
		wlr_log(WLR_ERROR,
			"CRTC %"PRIu32" is missing the OUT_FENCE_PTR property",
			crtc->id);
		atom->failed = true;
		return;
	}

	atomic_add(atom, crtc->id, crtc->props.out_fence_ptr, (uintptr_t)fd_ptr);
}

static void atomic_connector_add(struct atomic *atom,
		struct wlr_drm_connector_state *state, bool modeset) {
	struct wlr_drm_connector *conn = state->connector;
	struct wlr_drm_backend *drm = conn->backend;
	struct wlr_drm_crtc *crtc = conn->crtc;
	bool active = state->active;

	atomic_add(atom, conn->id, conn->props.crtc_id, active ? crtc->id : 0);
	if (modeset && active && conn->props.link_status != 0) {
		atomic_add(atom, conn->id, conn->props.link_status,
			DRM_MODE_LINK_STATUS_GOOD);
	}
	if (active && conn->props.content_type != 0) {
		atomic_add(atom, conn->id, conn->props.content_type,
			DRM_MODE_CONTENT_TYPE_GRAPHICS);
	}
	if (modeset && active && conn->props.max_bpc != 0 && conn->max_bpc_bounds[1] != 0) {
		atomic_add(atom, conn->id, conn->props.max_bpc, pick_max_bpc(conn, state->primary_fb));
	}
	if (conn->props.colorspace != 0) {
		atomic_add(atom, conn->id, conn->props.colorspace, state->colorspace);
	}
	if (conn->props.hdr_output_metadata != 0) {
		atomic_add(atom, conn->id, conn->props.hdr_output_metadata, state->hdr_output_metadata);
	}
	atomic_add(atom, crtc->id, crtc->props.mode_id, state->mode_id);
	atomic_add(atom, crtc->id, crtc->props.active, active);
	if (active) {
		if (crtc->props.gamma_lut != 0) {
			atomic_add(atom, crtc->id, crtc->props.gamma_lut, state->gamma_lut);
		}
		if (crtc->props.vrr_enabled != 0) {
			atomic_add(atom, crtc->id, crtc->props.vrr_enabled, state->vrr_enabled);
		}

		set_plane_props(atom, drm, crtc->primary, state->primary_fb, crtc->id,
			&state->primary_viewport.dst_box, &state->primary_viewport.src_box);
		if (crtc->primary->props.fb_damage_clips != 0) {
			atomic_add(atom, crtc->primary->id,
				crtc->primary->props.fb_damage_clips, state->fb_damage_clips);
		}
		if (state->primary_in_fence_fd >= 0) {
			set_plane_in_fence_fd(atom, crtc->primary, state->primary_in_fence_fd);
		}
		if (state->base->committed & WLR_OUTPUT_STATE_SIGNAL_TIMELINE) {
			set_crtc_out_fence_ptr(atom, crtc, &state->out_fence_fd);
		}
		if (crtc->cursor) {
			if (drm_connector_is_cursor_visible(conn)) {
				struct wlr_fbox cursor_src = {
					.width = state->cursor_fb->wlr_buf->width,
					.height = state->cursor_fb->wlr_buf->height,
				};
				struct wlr_box cursor_dst = {
					.x = conn->cursor_x,
					.y = conn->cursor_y,
					.width = state->cursor_fb->wlr_buf->width,
					.height = state->cursor_fb->wlr_buf->height,
				};
				set_plane_props(atom, drm, crtc->cursor, state->cursor_fb,
					crtc->id, &cursor_dst, &cursor_src);
				if (supports_cursor_hotspots(crtc->cursor)) {
					atomic_add(atom, crtc->cursor->id,
						crtc->cursor->props.hotspot_x, conn->cursor_hotspot_x);
					atomic_add(atom, crtc->cursor->id,
						crtc->cursor->props.hotspot_y, conn->cursor_hotspot_y);
				}
			} else {
				plane_disable(atom, crtc->cursor);
			}
		}
	} else {
		plane_disable(atom, crtc->primary);
		if (crtc->cursor) {
			plane_disable(atom, crtc->cursor);
		}
	}
}

static bool atomic_device_commit(struct wlr_drm_backend *drm,
		const struct wlr_drm_device_state *state,
		struct wlr_drm_page_flip *page_flip, uint32_t flags, bool test_only) {
	bool ok = false;

	for (size_t i = 0; i < state->connectors_len; i++) {
		if (!drm_atomic_connector_prepare(&state->connectors[i], state->modeset)) {
			goto out;
		}
	}

	struct atomic atom;
	atomic_begin(&atom);

	for (size_t i = 0; i < state->connectors_len; i++) {
		atomic_connector_add(&atom, &state->connectors[i], state->modeset);
	}

	if (test_only) {
		flags |= DRM_MODE_ATOMIC_TEST_ONLY;
	}
	if (state->modeset) {
		flags |= DRM_MODE_ATOMIC_ALLOW_MODESET;
	}
	if (state->nonblock) {
		flags |= DRM_MODE_ATOMIC_NONBLOCK;
	}

	ok = atomic_commit(&atom, drm, state, page_flip, flags);
	atomic_finish(&atom);

out:
	for (size_t i = 0; i < state->connectors_len; i++) {
		struct wlr_drm_connector_state *conn_state = &state->connectors[i];
		if (ok && !test_only) {
			drm_atomic_connector_apply_commit(conn_state);
		} else {
			drm_atomic_connector_rollback_commit(conn_state);
		}
	}
	return ok;
}

const struct wlr_drm_interface atomic_iface = {
	.commit = atomic_device_commit,
};
