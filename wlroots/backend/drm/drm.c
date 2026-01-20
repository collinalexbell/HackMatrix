#include <assert.h>
#include <drm_fourcc.h>
#include <drm_mode.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/backend/interface.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include <wlr/util/transform.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "backend/drm/drm.h"
#include "backend/drm/fb.h"
#include "backend/drm/iface.h"
#include "backend/drm/util.h"
#include "render/color.h"
#include "types/wlr_output.h"
#include "util/env.h"
#include "config.h"

#if HAVE_LIBLIFTOFF
#include <libliftoff.h>
#endif

// Output state which needs a KMS commit to be applied
static const uint32_t COMMIT_OUTPUT_STATE =
	WLR_OUTPUT_STATE_BUFFER |
	WLR_OUTPUT_STATE_MODE |
	WLR_OUTPUT_STATE_ENABLED |
	WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED |
	WLR_OUTPUT_STATE_LAYERS |
	WLR_OUTPUT_STATE_WAIT_TIMELINE |
	WLR_OUTPUT_STATE_SIGNAL_TIMELINE |
	WLR_OUTPUT_STATE_COLOR_TRANSFORM |
	WLR_OUTPUT_STATE_IMAGE_DESCRIPTION;

static const uint32_t SUPPORTED_OUTPUT_STATE =
	WLR_OUTPUT_STATE_BACKEND_OPTIONAL | COMMIT_OUTPUT_STATE;

bool check_drm_features(struct wlr_drm_backend *drm) {
	if (drmGetCap(drm->fd, DRM_CAP_CURSOR_WIDTH, &drm->cursor_width)) {
		drm->cursor_width = 64;
	}
	if (drmGetCap(drm->fd, DRM_CAP_CURSOR_HEIGHT, &drm->cursor_height)) {
		drm->cursor_height = 64;
	}

	uint64_t cap;
	if (drmGetCap(drm->fd, DRM_CAP_PRIME, &cap) ||
			!(cap & DRM_PRIME_CAP_IMPORT)) {
		wlr_log(WLR_ERROR, "PRIME import not supported");
		return false;
	}

	if (drm->parent) {
		if (drmGetCap(drm->parent->fd, DRM_CAP_PRIME, &cap) ||
				!(cap & DRM_PRIME_CAP_EXPORT)) {
			wlr_log(WLR_ERROR,
				"PRIME export not supported on primary GPU");
			return false;
		}
	}

	if (drmSetClientCap(drm->fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1)) {
		wlr_log(WLR_ERROR, "DRM universal planes unsupported");
		return false;
	}

	if (drmGetCap(drm->fd, DRM_CAP_CRTC_IN_VBLANK_EVENT, &cap) || !cap) {
		wlr_log(WLR_ERROR, "DRM_CRTC_IN_VBLANK_EVENT unsupported");
		return false;
	}

	if (drmGetCap(drm->fd, DRM_CAP_TIMESTAMP_MONOTONIC, &cap) || !cap) {
		wlr_log(WLR_ERROR, "DRM_CAP_TIMESTAMP_MONOTONIC unsupported");
		return false;
	}

	if (env_parse_bool("WLR_DRM_FORCE_LIBLIFTOFF")) {
#if HAVE_LIBLIFTOFF
		wlr_log(WLR_INFO,
			"WLR_DRM_FORCE_LIBLIFTOFF set, forcing libliftoff interface");
		if (drmSetClientCap(drm->fd, DRM_CLIENT_CAP_ATOMIC, 1) != 0) {
			wlr_log_errno(WLR_ERROR, "drmSetClientCap(ATOMIC) failed");
			return false;
		}
		drm->iface = &liftoff_iface;
#else
		wlr_log(WLR_ERROR, "libliftoff interface not available");
		return false;
#endif
	} else if (env_parse_bool("WLR_DRM_NO_ATOMIC")) {
		wlr_log(WLR_DEBUG,
			"WLR_DRM_NO_ATOMIC set, forcing legacy DRM interface");
		drm->iface = &legacy_iface;
	} else if (drmSetClientCap(drm->fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
		wlr_log(WLR_DEBUG,
			"Atomic modesetting unsupported, using legacy DRM interface");
		drm->iface = &legacy_iface;
	} else {
		wlr_log(WLR_DEBUG, "Using atomic DRM interface");
		drm->iface = &atomic_iface;
	}
#ifdef DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT
	if (drm->iface == &atomic_iface && drmSetClientCap(drm->fd, DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT, 1) == 0) {
		wlr_log(WLR_INFO, "DRM_CLIENT_CAP_CURSOR_PLANE_HOTSPOT supported");
	}
#endif

	if (drm->iface == &legacy_iface) {
		drm->supports_tearing_page_flips = drmGetCap(drm->fd, DRM_CAP_ASYNC_PAGE_FLIP, &cap) == 0 && cap == 1;
	} else {
		drm->supports_tearing_page_flips = drmGetCap(drm->fd, DRM_CAP_ATOMIC_ASYNC_PAGE_FLIP, &cap) == 0 && cap == 1;
		drm->backend.features.timeline = drmGetCap(drm->fd, DRM_CAP_SYNCOBJ_TIMELINE, &cap) == 0 && cap == 1;
	}

	if (env_parse_bool("WLR_DRM_NO_MODIFIERS")) {
		wlr_log(WLR_DEBUG, "WLR_DRM_NO_MODIFIERS set, disabling modifiers");
	} else {
		int ret = drmGetCap(drm->fd, DRM_CAP_ADDFB2_MODIFIERS, &cap);
		drm->addfb2_modifiers = ret == 0 && cap == 1;
		wlr_log(WLR_DEBUG, "ADDFB2 modifiers %s",
			drm->addfb2_modifiers ? "supported" : "unsupported");
	}

	return true;
}

static bool init_plane_cursor_sizes(struct wlr_drm_plane *plane,
		const struct drm_plane_size_hint *hints, size_t hints_len) {
	assert(hints_len > 0);
	plane->cursor_sizes = calloc(hints_len, sizeof(plane->cursor_sizes[0]));
	if (plane->cursor_sizes == NULL) {
		return false;
	}
	plane->cursor_sizes_len = hints_len;

	for (size_t i = 0; i < hints_len; i++) {
		const struct drm_plane_size_hint hint = hints[i];
		plane->cursor_sizes[i] = (struct wlr_output_cursor_size){
			.width = hint.width,
			.height = hint.height,
		};
	}

	return true;
}

static bool init_plane(struct wlr_drm_backend *drm,
		struct wlr_drm_plane *p, const drmModePlane *drm_plane) {
	uint32_t id = drm_plane->plane_id;

	struct wlr_drm_plane_props props = {0};
	if (!get_drm_plane_props(drm->fd, id, &props)) {
		return false;
	}

	uint64_t type;
	if (!get_drm_prop(drm->fd, id, props.type, &type)) {
		return false;
	}

	p->type = type;
	p->id = id;
	p->props = props;
	p->initial_crtc_id = drm_plane->crtc_id;

	for (size_t i = 0; i < drm_plane->count_formats; ++i) {
		// Force a LINEAR layout for the cursor if the driver doesn't support
		// modifiers
		wlr_drm_format_set_add(&p->formats, drm_plane->formats[i],
			DRM_FORMAT_MOD_LINEAR);
		if (type != DRM_PLANE_TYPE_CURSOR) {
			wlr_drm_format_set_add(&p->formats, drm_plane->formats[i],
				DRM_FORMAT_MOD_INVALID);
		}
	}

	if (p->props.in_formats && drm->addfb2_modifiers) {
		uint64_t blob_id;
		if (!get_drm_prop(drm->fd, p->id, p->props.in_formats, &blob_id)) {
			wlr_log(WLR_ERROR, "Failed to read IN_FORMATS property");
			return false;
		}

		drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(drm->fd, blob_id);
		if (!blob) {
			wlr_log(WLR_ERROR, "Failed to read IN_FORMATS blob");
			return false;
		}

		drmModeFormatModifierIterator iter = {0};
		while (drmModeFormatModifierBlobIterNext(blob, &iter)) {
			wlr_drm_format_set_add(&p->formats, iter.fmt, iter.mod);
		}

		drmModeFreePropertyBlob(blob);
	}

	uint64_t size_hints_blob_id = 0;
	if (p->props.size_hints) {
		if (!get_drm_prop(drm->fd, p->id, p->props.size_hints, &size_hints_blob_id)) {
			wlr_log(WLR_ERROR, "Failed to read SIZE_HINTS property");
			return false;
		}
	}
	if (size_hints_blob_id != 0) {
		drmModePropertyBlobRes *blob = drmModeGetPropertyBlob(drm->fd, size_hints_blob_id);
		if (!blob) {
			wlr_log(WLR_ERROR, "Failed to read SIZE_HINTS blob");
			return false;
		}

		const struct drm_plane_size_hint *size_hints = blob->data;
		size_t size_hints_len = blob->length / sizeof(size_hints[0]);
		if (!init_plane_cursor_sizes(p, size_hints, size_hints_len)) {
			return false;
		}

		drmModeFreePropertyBlob(blob);
	} else {
		const struct drm_plane_size_hint size_hint = {
			.width = drm->cursor_width,
			.height = drm->cursor_height,
		};
		if (!init_plane_cursor_sizes(p, &size_hint, 1)) {
			return false;
		}
	}

	assert(drm->num_crtcs <= 32);
	for (size_t j = 0; j < drm->num_crtcs; j++) {
		uint32_t crtc_bit = 1 << j;
		if ((drm_plane->possible_crtcs & crtc_bit) == 0) {
			continue;
		}

		struct wlr_drm_crtc *crtc = &drm->crtcs[j];
		if (type == DRM_PLANE_TYPE_PRIMARY && !crtc->primary) {
			crtc->primary = p;
			break;
		}
		if (type == DRM_PLANE_TYPE_CURSOR && !crtc->cursor) {
			crtc->cursor = p;
			break;
		}
	}

	return true;
}

static bool init_planes(struct wlr_drm_backend *drm) {
	drmModePlaneRes *plane_res = drmModeGetPlaneResources(drm->fd);
	if (!plane_res) {
		wlr_log_errno(WLR_ERROR, "Failed to get DRM plane resources");
		return false;
	}

	wlr_log(WLR_INFO, "Found %"PRIu32" DRM planes", plane_res->count_planes);

	drm->num_planes = plane_res->count_planes;
	drm->planes = calloc(drm->num_planes, sizeof(*drm->planes));
	if (drm->planes == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		goto error;
	}

	for (uint32_t i = 0; i < plane_res->count_planes; ++i) {
		uint32_t id = plane_res->planes[i];

		drmModePlane *drm_plane = drmModeGetPlane(drm->fd, id);
		if (!drm_plane) {
			wlr_log_errno(WLR_ERROR, "Failed to get DRM plane");
			goto error;
		}

		struct wlr_drm_plane *plane = &drm->planes[i];
		if (!init_plane(drm, plane, drm_plane)) {
			goto error;
		}

		drmModeFreePlane(drm_plane);
	}

	drmModeFreePlaneResources(plane_res);
	return true;

error:
	free(drm->planes);
	drmModeFreePlaneResources(plane_res);
	return false;
}

bool init_drm_resources(struct wlr_drm_backend *drm) {
	drmModeRes *res = drmModeGetResources(drm->fd);
	if (!res) {
		wlr_log_errno(WLR_ERROR, "Failed to get DRM resources");
		return false;
	}

	wlr_log(WLR_INFO, "Found %d DRM CRTCs", res->count_crtcs);

	drm->num_crtcs = res->count_crtcs;
	if (drm->num_crtcs == 0) {
		drmModeFreeResources(res);
		return true;
	}

	drm->crtcs = calloc(drm->num_crtcs, sizeof(drm->crtcs[0]));
	if (!drm->crtcs) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		goto error_res;
	}

	for (size_t i = 0; i < drm->num_crtcs; ++i) {
		struct wlr_drm_crtc *crtc = &drm->crtcs[i];
		crtc->id = res->crtcs[i];

		drmModeCrtc *drm_crtc = drmModeGetCrtc(drm->fd, crtc->id);
		if (drm_crtc == NULL) {
			wlr_log_errno(WLR_ERROR, "drmModeGetCrtc failed");
			goto error_res;
		}
		crtc->legacy_gamma_size = drm_crtc->gamma_size;
		drmModeFreeCrtc(drm_crtc);

		if (!get_drm_crtc_props(drm->fd, crtc->id, &crtc->props)) {
			goto error_crtcs;
		}

		wl_list_init(&crtc->layers);
	}

	if (!init_planes(drm)) {
		goto error_crtcs;
	}

	if (drm->iface->init != NULL && !drm->iface->init(drm)) {
		goto error_crtcs;
	}

	drmModeFreeResources(res);

	return true;

error_crtcs:
	free(drm->crtcs);
error_res:
	drmModeFreeResources(res);
	return false;
}

static void drm_plane_finish_surface(struct wlr_drm_plane *plane) {
	if (!plane) {
		return;
	}

	drm_fb_clear(&plane->queued_fb);
	drm_fb_clear(&plane->current_fb);

	finish_drm_surface(&plane->mgpu_surf);
}

void finish_drm_resources(struct wlr_drm_backend *drm) {
	if (!drm) {
		return;
	}

	if (drm->iface->finish != NULL) {
		drm->iface->finish(drm);
	}

	for (size_t i = 0; i < drm->num_crtcs; ++i) {
		struct wlr_drm_crtc *crtc = &drm->crtcs[i];

		if (crtc->mode_id && crtc->own_mode_id) {
			drmModeDestroyPropertyBlob(drm->fd, crtc->mode_id);
		}
		if (crtc->gamma_lut) {
			drmModeDestroyPropertyBlob(drm->fd, crtc->gamma_lut);
		}
	}

	free(drm->crtcs);

	for (size_t i = 0; i < drm->num_planes; ++i) {
		struct wlr_drm_plane *plane = &drm->planes[i];
		drm_plane_finish_surface(plane);
		wlr_drm_format_set_finish(&plane->formats);
		free(plane->cursor_sizes);
	}

	free(drm->planes);
}

static struct wlr_drm_connector *get_drm_connector_from_output(
		struct wlr_output *wlr_output) {
	assert(wlr_output_is_drm(wlr_output));
	struct wlr_drm_connector *conn = wl_container_of(wlr_output, conn, output);
	return conn;
}

static void layer_handle_addon_destroy(struct wlr_addon *addon) {
	struct wlr_drm_layer *layer = wl_container_of(addon, layer, addon);
	wlr_addon_finish(&layer->addon);
	wl_list_remove(&layer->link);
#if HAVE_LIBLIFTOFF
	liftoff_layer_destroy(layer->liftoff);
#endif
	drm_fb_clear(&layer->pending_fb);
	drm_fb_clear(&layer->queued_fb);
	drm_fb_clear(&layer->current_fb);
	free(layer->candidate_planes);
	free(layer);
}

const struct wlr_addon_interface layer_impl = {
	.name = "wlr_drm_layer",
	.destroy = layer_handle_addon_destroy,
};

struct wlr_drm_layer *get_drm_layer(struct wlr_drm_backend *drm,
		struct wlr_output_layer *wlr_layer) {
	struct wlr_addon *addon =
		wlr_addon_find(&wlr_layer->addons, drm, &layer_impl);
	assert(addon != NULL);
	struct wlr_drm_layer *layer = wl_container_of(addon, layer, addon);
	return layer;
}

static struct wlr_drm_layer *get_or_create_layer(struct wlr_drm_backend *drm,
		struct wlr_drm_crtc *crtc, struct wlr_output_layer *wlr_layer) {
	struct wlr_drm_layer *layer;
	struct wlr_addon *addon =
		wlr_addon_find(&wlr_layer->addons, drm, &layer_impl);
	if (addon != NULL) {
		layer = wl_container_of(addon, layer, addon);
		return layer;
	}

	layer = calloc(1, sizeof(*layer));
	if (layer == NULL) {
		return NULL;
	}

	layer->wlr = wlr_layer;

#if HAVE_LIBLIFTOFF
	layer->liftoff = liftoff_layer_create(crtc->liftoff);
	if (layer->liftoff == NULL) {
		free(layer);
		return NULL;
	}
#else
	abort(); // unreachable
#endif

	layer->candidate_planes = calloc(drm->num_planes, sizeof(layer->candidate_planes[0]));
	if (layer->candidate_planes == NULL) {
#if HAVE_LIBLIFTOFF
		liftoff_layer_destroy(layer->liftoff);
#endif
		free(layer);
		return NULL;
	}

	wlr_addon_init(&layer->addon, &wlr_layer->addons, drm, &layer_impl);
	wl_list_insert(&crtc->layers, &layer->link);

	return layer;
}

void drm_page_flip_destroy(struct wlr_drm_page_flip *page_flip) {
	if (!page_flip) {
		return;
	}

	wl_list_remove(&page_flip->link);
	free(page_flip->connectors);
	free(page_flip);
}

static struct wlr_drm_page_flip *drm_page_flip_create(struct wlr_drm_backend *drm,
		const struct wlr_drm_device_state *state) {
	struct wlr_drm_page_flip *page_flip = calloc(1, sizeof(*page_flip));
	if (page_flip == NULL) {
		return NULL;
	}
	page_flip->connectors_len = state->connectors_len;
	page_flip->connectors =
		calloc(page_flip->connectors_len, sizeof(page_flip->connectors[0]));
	if (page_flip->connectors == NULL) {
		free(page_flip);
		return NULL;
	}
	for (size_t i = 0; i < state->connectors_len; i++) {
		struct wlr_drm_connector *conn = state->connectors[i].connector;
		page_flip->connectors[i] = (struct wlr_drm_page_flip_connector){
			.connector = conn,
			.crtc_id = conn->crtc->id,
		};
	}
	wl_list_insert(&drm->page_flips, &page_flip->link);
	return page_flip;
}

static struct wlr_drm_connector *drm_page_flip_pop(
		struct wlr_drm_page_flip *page_flip, uint32_t crtc_id) {
	bool found = false;
	size_t i;
	for (i = 0; i < page_flip->connectors_len; i++) {
		if (page_flip->connectors[i].crtc_id == crtc_id) {
			found = true;
			break;
		}
	}
	if (!found) {
		return NULL;
	}

	struct wlr_drm_connector *conn = page_flip->connectors[i].connector;
	if (i != page_flip->connectors_len - 1) {
		page_flip->connectors[i] = page_flip->connectors[page_flip->connectors_len - 1];
	}
	page_flip->connectors_len--;
	return conn;
}

static void drm_connector_set_pending_page_flip(struct wlr_drm_connector *conn,
		struct wlr_drm_page_flip *page_flip) {
	if (conn->pending_page_flip != NULL) {
		struct wlr_drm_page_flip *page_flip = conn->pending_page_flip;
		for (size_t i = 0; i < page_flip->connectors_len; i++) {
			if (page_flip->connectors[i].connector == conn) {
				page_flip->connectors[i].connector = NULL;
			}
		}
	}
	conn->pending_page_flip = page_flip;
}

static void drm_connector_apply_commit(const struct wlr_drm_connector_state *state,
		struct wlr_drm_page_flip *page_flip) {
	struct wlr_drm_connector *conn = state->connector;
	struct wlr_drm_crtc *crtc = conn->crtc;

	drm_fb_copy(&crtc->primary->queued_fb, state->primary_fb);
	crtc->primary->viewport = state->primary_viewport;
	if (crtc->cursor != NULL) {
		drm_fb_copy(&crtc->cursor->queued_fb, state->cursor_fb);
	}
	drm_fb_clear(&conn->cursor_pending_fb);

	struct wlr_drm_layer *layer;
	wl_list_for_each(layer, &crtc->layers, link) {
		drm_fb_move(&layer->queued_fb, &layer->pending_fb);
	}

	drm_connector_set_pending_page_flip(conn, page_flip);

	if (state->base->committed & WLR_OUTPUT_STATE_MODE) {
		conn->refresh = calculate_refresh_rate(&state->mode);
	}

	if (!state->active) {
		drm_plane_finish_surface(crtc->primary);
		drm_plane_finish_surface(crtc->cursor);
		drm_fb_clear(&conn->cursor_pending_fb);

		conn->cursor_enabled = false;
		conn->crtc = NULL;

		// Legacy uAPI doesn't support requesting page-flip events when
		// turning off a CRTC
		if (page_flip != NULL && conn->backend->iface == &legacy_iface) {
			drm_page_flip_pop(page_flip, crtc->id);
			conn->pending_page_flip = NULL;
			if (page_flip->connectors_len == 0) {
				drm_page_flip_destroy(page_flip);
			}
		}
	}
}

static void drm_connector_rollback_commit(const struct wlr_drm_connector_state *state) {
	struct wlr_drm_crtc *crtc = state->connector->crtc;

	// The set_cursor() hook is a bit special: it's not really synchronized
	// to commit() or test(). Once set_cursor() returns true, the new
	// cursor is effectively committed. So don't roll it back here, or we
	// risk ending up in a state where we don't have a cursor FB but
	// wlr_drm_connector.cursor_enabled is true.
	// TODO: fix our output interface to avoid this issue.

	struct wlr_drm_layer *layer;
	wl_list_for_each(layer, &crtc->layers, link) {
		drm_fb_clear(&layer->pending_fb);
	}
}

static bool drm_commit(struct wlr_drm_backend *drm,
		const struct wlr_drm_device_state *state,
		uint32_t flags, bool test_only) {
	// Disallow atomic-only flags
	assert((flags & ~DRM_MODE_PAGE_FLIP_FLAGS) == 0);

	struct wlr_drm_page_flip *page_flip = NULL;
	if (flags & DRM_MODE_PAGE_FLIP_EVENT) {
		page_flip = drm_page_flip_create(drm, state);
		if (page_flip == NULL) {
			return false;
		}
		page_flip->async = (flags & DRM_MODE_PAGE_FLIP_ASYNC);
	}

	bool ok = drm->iface->commit(drm, state, page_flip, flags, test_only);
	if (ok && !test_only) {
		for (size_t i = 0; i < state->connectors_len; i++) {
			drm_connector_apply_commit(&state->connectors[i], page_flip);
		}
	} else {
		for (size_t i = 0; i < state->connectors_len; i++) {
			drm_connector_rollback_commit(&state->connectors[i]);
		}
		drm_page_flip_destroy(page_flip);
	}
	return ok;
}

static void drm_connector_state_init(struct wlr_drm_connector_state *state,
		struct wlr_drm_connector *conn,
		const struct wlr_output_state *base) {
	*state = (struct wlr_drm_connector_state){
		.connector = conn,
		.base = base,
		.active = output_pending_enabled(&conn->output, base),
		.primary_in_fence_fd = -1,
		.out_fence_fd = -1,
	};

	struct wlr_output_mode *mode = conn->output.current_mode;
	int32_t width = conn->output.width;
	int32_t height = conn->output.height;
	int32_t refresh = conn->output.refresh;

	if (base->committed & WLR_OUTPUT_STATE_MODE) {
		switch (base->mode_type) {
		case WLR_OUTPUT_STATE_MODE_FIXED:
			mode = base->mode;
			break;
		case WLR_OUTPUT_STATE_MODE_CUSTOM:
			mode = NULL;
			width = base->custom_mode.width;
			height = base->custom_mode.height;
			refresh = base->custom_mode.refresh;
			break;
		}
	}

	if (mode) {
		struct wlr_drm_mode *drm_mode = wl_container_of(mode, drm_mode, wlr_mode);
		state->mode = drm_mode->drm_mode;
	} else {
		generate_cvt_mode(&state->mode, width, height, (float)refresh / 1000);
		state->mode.type = DRM_MODE_TYPE_USERDEF;
	}

	if (output_pending_enabled(&conn->output, base)) {
		// The CRTC must be set up before this function is called
		assert(conn->crtc != NULL);

		struct wlr_drm_plane *primary = conn->crtc->primary;
		if (primary->queued_fb != NULL) {
			state->primary_fb = drm_fb_lock(primary->queued_fb);
			state->primary_viewport = primary->viewport;
		} else if (primary->current_fb != NULL) {
			state->primary_fb = drm_fb_lock(primary->current_fb);
			state->primary_viewport = primary->viewport;
		}

		if (conn->cursor_enabled) {
			struct wlr_drm_plane *cursor = conn->crtc->cursor;
			assert(cursor != NULL);
			if (conn->cursor_pending_fb != NULL) {
				state->cursor_fb = drm_fb_lock(conn->cursor_pending_fb);
			} else if (cursor->queued_fb != NULL) {
				state->cursor_fb = drm_fb_lock(cursor->queued_fb);
			} else if (cursor->current_fb != NULL) {
				state->cursor_fb = drm_fb_lock(cursor->current_fb);
			}
		}
	}
}

static void drm_connector_state_finish(struct wlr_drm_connector_state *state) {
	drm_fb_clear(&state->primary_fb);
	drm_fb_clear(&state->cursor_fb);
	wlr_drm_syncobj_timeline_unref(state->wait_timeline);
}

static bool drm_connector_state_update_primary_fb(struct wlr_drm_connector *conn,
		struct wlr_drm_connector_state *state) {
	struct wlr_drm_backend *drm = conn->backend;

	assert(state->base->committed & WLR_OUTPUT_STATE_BUFFER);

	struct wlr_drm_crtc *crtc = conn->crtc;
	assert(crtc != NULL);

	struct wlr_drm_plane *plane = crtc->primary;
	struct wlr_buffer *source_buf = state->base->buffer;

	struct wlr_drm_syncobj_timeline *wait_timeline = NULL;
	uint64_t wait_point = 0;
	if (state->base->committed & WLR_OUTPUT_STATE_WAIT_TIMELINE) {
		wait_timeline = state->base->wait_timeline;
		wait_point = state->base->wait_point;
	}
	assert(state->wait_timeline == NULL);

	struct wlr_buffer *local_buf;
	if (drm->mgpu_renderer.wlr_rend) {
		struct wlr_drm_format format = {0};
		if (!drm_plane_pick_render_format(plane, &format, &drm->mgpu_renderer)) {
			wlr_log(WLR_ERROR, "Failed to pick primary plane format");
			return false;
		}

		// TODO: fallback to modifier-less buffer allocation
		bool ok = init_drm_surface(&plane->mgpu_surf, &drm->mgpu_renderer,
			source_buf->width, source_buf->height, &format);
		wlr_drm_format_finish(&format);
		if (!ok) {
			return false;
		}

		local_buf = drm_surface_blit(&plane->mgpu_surf, source_buf,
			wait_timeline, wait_point);
		if (local_buf == NULL) {
			return false;
		}

		if (plane->mgpu_surf.timeline != NULL) {
			state->wait_timeline = wlr_drm_syncobj_timeline_ref(plane->mgpu_surf.timeline);
			state->wait_point = plane->mgpu_surf.point;
		}
	} else {
		local_buf = wlr_buffer_lock(source_buf);

		if (wait_timeline != NULL) {
			state->wait_timeline = wlr_drm_syncobj_timeline_ref(wait_timeline);
			state->wait_point = wait_point;
		}
	}

	bool ok = drm_fb_import(&state->primary_fb, drm, local_buf,
		&plane->formats);
	wlr_buffer_unlock(local_buf);
	if (!ok) {
		wlr_drm_conn_log(conn, WLR_DEBUG,
			"Failed to import buffer for scan-out");
		return false;
	}

	output_state_get_buffer_src_box(state->base, &state->primary_viewport.src_box);
	output_state_get_buffer_dst_box(state->base, &state->primary_viewport.dst_box);

	return true;
}

static bool drm_connector_set_pending_layer_fbs(struct wlr_drm_connector *conn,
		const struct wlr_output_state *state) {
	struct wlr_drm_backend *drm = conn->backend;

	struct wlr_drm_crtc *crtc = conn->crtc;
	if (!crtc || drm->mgpu_renderer.wlr_rend) {
		return false;
	}

	if (!crtc->liftoff) {
		return true; // libliftoff is disabled
	}

	assert(state->committed & WLR_OUTPUT_STATE_LAYERS);

	for (size_t i = 0; i < state->layers_len; i++) {
		struct wlr_output_layer_state *layer_state = &state->layers[i];
		struct wlr_drm_layer *layer =
			get_or_create_layer(drm, crtc, layer_state->layer);
		if (!layer) {
			return false;
		}

		if (layer_state->buffer != NULL) {
			drm_fb_import(&layer->pending_fb, drm, layer_state->buffer, NULL);
		} else {
			drm_fb_clear(&layer->pending_fb);
		}
	}

	return true;
}

static bool drm_connector_alloc_crtc(struct wlr_drm_connector *conn);

static bool drm_connector_prepare(struct wlr_drm_connector_state *conn_state, bool test_only) {
	const struct wlr_output_state *state = conn_state->base;
	struct wlr_drm_connector *conn = conn_state->connector;
	struct wlr_output *output = &conn->output;

	uint32_t unsupported = state->committed & ~SUPPORTED_OUTPUT_STATE;
	if (unsupported != 0) {
		wlr_log(WLR_DEBUG, "Unsupported output state fields: 0x%"PRIx32,
			unsupported);
		return false;
	}

	if ((state->committed & WLR_OUTPUT_STATE_ENABLED) && state->enabled &&
			output->width == 0 && output->height == 0 &&
			!(state->committed & WLR_OUTPUT_STATE_MODE)) {
		wlr_drm_conn_log(conn, WLR_DEBUG,
			"Can't enable an output without a mode");
		return false;
	}

	if ((state->committed & WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED) &&
			state->adaptive_sync_enabled &&
			!output->adaptive_sync_supported) {
		wlr_drm_conn_log(conn, WLR_DEBUG,
				"Can't enable adaptive sync: connector doesn't support VRR");
		return false;
	}

	if ((state->committed & WLR_OUTPUT_STATE_BUFFER) && conn->backend->mgpu_renderer.wlr_rend) {
		struct wlr_dmabuf_attributes dmabuf;
		if (!wlr_buffer_get_dmabuf(state->buffer, &dmabuf)) {
			wlr_drm_conn_log(conn, WLR_DEBUG, "Buffer is not a DMA-BUF");
			return false;
		}

		if (!wlr_drm_format_set_has(&conn->backend->mgpu_formats, dmabuf.format, dmabuf.modifier)) {
			wlr_drm_conn_log(conn, WLR_DEBUG,
				"Buffer format 0x%"PRIX32" with modifier 0x%"PRIX64" cannot be "
				"imported into multi-GPU renderer",
				dmabuf.format, dmabuf.modifier);
			return false;
		}
	}

	if ((state->committed & WLR_OUTPUT_STATE_COLOR_TRANSFORM) && state->color_transform != NULL &&
			state->color_transform->type != COLOR_TRANSFORM_LUT_3X1D) {
		wlr_drm_conn_log(conn, WLR_DEBUG,
			"Only 3x1D LUT color transforms are supported");
		return false;
	}

	if ((state->committed & WLR_OUTPUT_STATE_IMAGE_DESCRIPTION) &&
			conn->backend->iface != &atomic_iface) {
		wlr_log(WLR_DEBUG, "Image descriptions are only supported by the atomic interface");
		return false;
	}

	if (test_only && conn->backend->mgpu_renderer.wlr_rend) {
		// If we're running as a secondary GPU, we can't perform an atomic
		// commit without blitting a buffer.
		return true;
	}

	if (state->committed & WLR_OUTPUT_STATE_BUFFER) {
		if (!drm_connector_state_update_primary_fb(conn, conn_state)) {
			return false;
		}

		if (conn_state->base->tearing_page_flip && !conn->backend->supports_tearing_page_flips) {
			wlr_log(WLR_ERROR, "Attempted to submit a tearing page flip to an unsupported backend!");
			return false;
		}
	}
	if (state->committed & WLR_OUTPUT_STATE_LAYERS) {
		if (!drm_connector_set_pending_layer_fbs(conn, conn_state->base)) {
			return false;
		}
	}

	if (conn_state->active && !conn_state->primary_fb) {
		wlr_drm_conn_log(conn, WLR_DEBUG,
			"No primary frame buffer available for this connector");
		return false;
	}

	return true;
}

static bool drm_connector_commit_state(struct wlr_drm_connector *conn,
		const struct wlr_output_state *state, bool test_only) {
	struct wlr_drm_backend *drm = conn->backend;

	if (!drm->session->active) {
		return false;
	}

	if (test_only && (state->committed & COMMIT_OUTPUT_STATE) == 0) {
		// This commit doesn't change the KMS state
		return true;
	}

	if (output_pending_enabled(&conn->output, state) && !drm_connector_alloc_crtc(conn)) {
		wlr_drm_conn_log(conn, WLR_DEBUG,
			"No CRTC available for this connector");
		return false;
	}

	bool ok = false;
	struct wlr_drm_connector_state pending = {0};
	drm_connector_state_init(&pending, conn, state);
	struct wlr_drm_device_state pending_dev = {
		.modeset = state->allow_reconfiguration,
		// The wlr_output API requires non-modeset commits with a new buffer to
		// wait for the frame event. However compositors often perform
		// non-modesets commits without a new buffer without waiting for the
		// frame event. In that case we need to make the KMS commit blocking,
		// otherwise the kernel will error out with EBUSY.
		.nonblock = !state->allow_reconfiguration &&
			(state->committed & WLR_OUTPUT_STATE_BUFFER),
		.connectors = &pending,
		.connectors_len = 1,
	};

	if (!drm_connector_prepare(&pending, test_only)) {
		goto out;
	}

	if (test_only && conn->backend->mgpu_renderer.wlr_rend) {
		// If we're running as a secondary GPU, we can't perform an atomic
		// commit without blitting a buffer.
		ok = true;
		goto out;
	}

	if (!pending.active && conn->crtc == NULL) {
		// Disabling an already-disabled connector
		ok = true;
		goto out;
	}

	if (!test_only && pending_dev.modeset) {
		if (pending.active) {
			wlr_drm_conn_log(conn, WLR_INFO, "Modesetting with %dx%d @ %.3f Hz",
				pending.mode.hdisplay, pending.mode.vdisplay,
				(float)calculate_refresh_rate(&pending.mode) / 1000);
		} else {
			wlr_drm_conn_log(conn, WLR_INFO, "Turning off");
		}
	}

	// wlr_drm_interface.crtc_commit will perform either a non-blocking
	// page-flip, either a blocking modeset. When performing a blocking modeset
	// we'll wait for all queued page-flips to complete, so we don't need this
	// safeguard.
	if (!test_only && pending_dev.nonblock && conn->pending_page_flip != NULL) {
		wlr_drm_conn_log(conn, WLR_ERROR, "Failed to page-flip output: "
			"a page-flip is already pending");
		goto out;
	}

	uint32_t flags = 0;
	if (!test_only && pending.active) {
		flags |= DRM_MODE_PAGE_FLIP_EVENT;
	}
	if (pending.base->tearing_page_flip) {
		flags |= DRM_MODE_PAGE_FLIP_ASYNC;
	}

	ok = drm_commit(drm, &pending_dev, flags, test_only);

out:
	drm_connector_state_finish(&pending);
	return ok;
}

static bool drm_connector_test(struct wlr_output *output,
		const struct wlr_output_state *state) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	return drm_connector_commit_state(conn, state, true);
}

static bool drm_connector_commit(struct wlr_output *output,
		const struct wlr_output_state *state) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	return drm_connector_commit_state(conn, state, false);
}

size_t drm_crtc_get_gamma_lut_size(struct wlr_drm_backend *drm,
		struct wlr_drm_crtc *crtc) {
	if (crtc->props.gamma_lut_size == 0 || drm->iface == &legacy_iface) {
		return (size_t)crtc->legacy_gamma_size;
	}

	uint64_t gamma_lut_size;
	if (!get_drm_prop(drm->fd, crtc->id, crtc->props.gamma_lut_size,
			&gamma_lut_size)) {
		wlr_log(WLR_ERROR, "Unable to get gamma lut size");
		return 0;
	}

	return gamma_lut_size;
}

static size_t drm_connector_get_gamma_size(struct wlr_output *output) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	struct wlr_drm_backend *drm = conn->backend;
	struct wlr_drm_crtc *crtc = conn->crtc;

	if (crtc == NULL) {
		return 0;
	}

	return drm_crtc_get_gamma_lut_size(drm, crtc);
}

static void realloc_crtcs(struct wlr_drm_backend *drm,
	struct wlr_drm_connector *want_conn);

static bool drm_connector_alloc_crtc(struct wlr_drm_connector *conn) {
	if (conn->crtc == NULL) {
		realloc_crtcs(conn->backend, conn);
	}
	bool ok = conn->crtc != NULL;
	if (!ok) {
		wlr_drm_conn_log(conn, WLR_DEBUG, "Failed to find free CRTC");
	}
	return ok;
}

static struct wlr_drm_mode *drm_mode_create(const drmModeModeInfo *modeinfo) {
	struct wlr_drm_mode *mode = calloc(1, sizeof(*mode));
	if (!mode) {
		return NULL;
	}

	mode->drm_mode = *modeinfo;
	mode->wlr_mode.width = mode->drm_mode.hdisplay;
	mode->wlr_mode.height = mode->drm_mode.vdisplay;
	mode->wlr_mode.refresh = calculate_refresh_rate(modeinfo);
	mode->wlr_mode.picture_aspect_ratio = get_picture_aspect_ratio(modeinfo);
	if (modeinfo->type & DRM_MODE_TYPE_PREFERRED) {
		mode->wlr_mode.preferred = true;
	}

	return mode;
}

struct wlr_output_mode *wlr_drm_connector_add_mode(struct wlr_output *output,
		const drmModeModeInfo *modeinfo) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);

	if (modeinfo->type != DRM_MODE_TYPE_USERDEF) {
		return NULL;
	}

	struct wlr_output_mode *wlr_mode;
	wl_list_for_each(wlr_mode, &conn->output.modes, link) {
		struct wlr_drm_mode *mode = wl_container_of(wlr_mode, mode, wlr_mode);
		if (memcmp(&mode->drm_mode, modeinfo, sizeof(*modeinfo)) == 0) {
			return wlr_mode;
		}
	}

	struct wlr_drm_mode *mode = drm_mode_create(modeinfo);
	if (!mode) {
		return NULL;
	}

	wl_list_insert(&conn->output.modes, &mode->wlr_mode.link);

	wlr_drm_conn_log(conn, WLR_INFO, "Registered custom mode "
			"%"PRId32"x%"PRId32"@%"PRId32,
			mode->wlr_mode.width, mode->wlr_mode.height,
			mode->wlr_mode.refresh);

	return &mode->wlr_mode;
}

const drmModeModeInfo *wlr_drm_mode_get_info(struct wlr_output_mode *wlr_mode) {
	const struct wlr_drm_mode *mode = wl_container_of(wlr_mode, mode, wlr_mode);
	return &mode->drm_mode;
}

static bool drm_connector_set_cursor(struct wlr_output *output,
		struct wlr_buffer *buffer, int hotspot_x, int hotspot_y) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	struct wlr_drm_backend *drm = conn->backend;
	struct wlr_drm_crtc *crtc = conn->crtc;

	if (!crtc) {
		return false;
	}

	struct wlr_drm_plane *plane = crtc->cursor;
	if (plane == NULL) {
		return false;
	}

	if (conn->cursor_hotspot_x != hotspot_x ||
			conn->cursor_hotspot_y != hotspot_y) {
		// Update cursor hotspot
		conn->cursor_x -= hotspot_x - conn->cursor_hotspot_x;
		conn->cursor_y -= hotspot_y - conn->cursor_hotspot_y;
		conn->cursor_hotspot_x = hotspot_x;
		conn->cursor_hotspot_y = hotspot_y;
	}

	conn->cursor_enabled = false;
	drm_fb_clear(&conn->cursor_pending_fb);
	if (buffer != NULL) {
		bool found = false;
		for (size_t i = 0; i < plane->cursor_sizes_len; i++) {
			struct wlr_output_cursor_size size = plane->cursor_sizes[i];
			if (size.width == buffer->width && size.height == buffer->height) {
				found = true;
				break;
			}
		}
		if (!found) {
			wlr_drm_conn_log(conn, WLR_DEBUG, "Cursor buffer size mismatch");
			return false;
		}

		struct wlr_buffer *local_buf;
		if (drm->mgpu_renderer.wlr_rend) {
			struct wlr_drm_format format = {0};
			if (!drm_plane_pick_render_format(plane, &format, &drm->mgpu_renderer)) {
				wlr_log(WLR_ERROR, "Failed to pick cursor plane format");
				return false;
			}

			bool ok = init_drm_surface(&plane->mgpu_surf, &drm->mgpu_renderer,
				buffer->width, buffer->height, &format);
			wlr_drm_format_finish(&format);
			if (!ok) {
				return false;
			}

			local_buf = drm_surface_blit(&plane->mgpu_surf, buffer, NULL, 0);
			if (local_buf == NULL) {
				return false;
			}
		} else {
			local_buf = wlr_buffer_lock(buffer);
		}

		bool ok = drm_fb_import(&conn->cursor_pending_fb, drm, local_buf,
			&plane->formats);
		wlr_buffer_unlock(local_buf);
		if (!ok) {
			return false;
		}

		conn->cursor_enabled = true;
		conn->cursor_width = buffer->width;
		conn->cursor_height = buffer->height;
	}

	return true;
}

static bool drm_connector_move_cursor(struct wlr_output *output,
		int x, int y) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	if (!conn->crtc) {
		return false;
	}
	struct wlr_drm_plane *plane = conn->crtc->cursor;
	if (!plane) {
		return false;
	}

	struct wlr_box box = { .x = x, .y = y };

	int width, height;
	wlr_output_transformed_resolution(output, &width, &height);

	enum wl_output_transform transform =
		wlr_output_transform_invert(output->transform);
	wlr_box_transform(&box, &box, transform, width, height);

	box.x -= conn->cursor_hotspot_x;
	box.y -= conn->cursor_hotspot_y;

	conn->cursor_x = box.x;
	conn->cursor_y = box.y;

	return true;
}

bool drm_connector_is_cursor_visible(struct wlr_drm_connector *conn) {
	return conn->cursor_enabled &&
		conn->cursor_x < conn->output.width &&
		conn->cursor_y < conn->output.height &&
		conn->cursor_x + conn->cursor_width >= 0 &&
		conn->cursor_y + conn->cursor_height >= 0;
}

static void dealloc_crtc(struct wlr_drm_connector *conn);

/**
 * Destroy the compositor-facing part of a connector.
 *
 * The connector isn't destroyed when disconnected. Only the compositor-facing
 * wlr_output interface is cleaned up.
 */
static void drm_connector_destroy_output(struct wlr_output *output) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);

	wlr_output_finish(output);

	dealloc_crtc(conn);

	conn->status = DRM_MODE_DISCONNECTED;
	drm_connector_set_pending_page_flip(conn, NULL);

	struct wlr_drm_mode *mode, *mode_tmp;
	wl_list_for_each_safe(mode, mode_tmp, &conn->output.modes, wlr_mode.link) {
		wl_list_remove(&mode->wlr_mode.link);
		free(mode);
	}

	conn->output = (struct wlr_output){0};
}

static const struct wlr_drm_format_set *drm_connector_get_cursor_formats(
		struct wlr_output *output, uint32_t buffer_caps) {
	if (!(buffer_caps & WLR_BUFFER_CAP_DMABUF)) {
		return NULL;
	}
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	if (!drm_connector_alloc_crtc(conn)) {
		return NULL;
	}
	struct wlr_drm_plane *plane = conn->crtc->cursor;
	if (!plane) {
		return NULL;
	}
	if (conn->backend->mgpu_renderer.wlr_rend) {
		return &conn->backend->mgpu_formats;
	}
	return &plane->formats;
}

static const struct wlr_output_cursor_size *drm_connector_get_cursor_sizes(struct wlr_output *output,
		size_t *len) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	if (!drm_connector_alloc_crtc(conn)) {
		return NULL;
	}
	struct wlr_drm_plane *plane = conn->crtc->cursor;
	if (!plane) {
		return NULL;
	}
	*len = plane->cursor_sizes_len;
	return plane->cursor_sizes;
}

static const struct wlr_drm_format_set *drm_connector_get_primary_formats(
		struct wlr_output *output, uint32_t buffer_caps) {
	if (!(buffer_caps & WLR_BUFFER_CAP_DMABUF)) {
		return NULL;
	}
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	if (!drm_connector_alloc_crtc(conn)) {
		return NULL;
	}
	if (conn->backend->mgpu_renderer.wlr_rend) {
		return &conn->backend->mgpu_formats;
	}
	return &conn->crtc->primary->formats;
}

static const struct wlr_output_impl output_impl = {
	.set_cursor = drm_connector_set_cursor,
	.move_cursor = drm_connector_move_cursor,
	.destroy = drm_connector_destroy_output,
	.test = drm_connector_test,
	.commit = drm_connector_commit,
	.get_gamma_size = drm_connector_get_gamma_size,
	.get_cursor_formats = drm_connector_get_cursor_formats,
	.get_cursor_sizes = drm_connector_get_cursor_sizes,
	.get_primary_formats = drm_connector_get_primary_formats,
};

bool wlr_output_is_drm(struct wlr_output *output) {
	return output->impl == &output_impl;
}

uint32_t wlr_drm_connector_get_id(struct wlr_output *output) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	return conn->id;
}

enum wl_output_transform wlr_drm_connector_get_panel_orientation(
		struct wlr_output *output) {
	struct wlr_drm_connector *conn = get_drm_connector_from_output(output);
	if (!conn->props.panel_orientation) {
		return WL_OUTPUT_TRANSFORM_NORMAL;
	}

	char *orientation = get_drm_prop_enum(conn->backend->fd, conn->id,
		conn->props.panel_orientation);
	if (orientation == NULL) {
		return WL_OUTPUT_TRANSFORM_NORMAL;
	}

	enum wl_output_transform tr;
	if (strcmp(orientation, "Normal") == 0) {
		tr = WL_OUTPUT_TRANSFORM_NORMAL;
	} else if (strcmp(orientation, "Left Side Up") == 0) {
		tr = WL_OUTPUT_TRANSFORM_90;
	} else if (strcmp(orientation, "Upside Down") == 0) {
		tr = WL_OUTPUT_TRANSFORM_180;
	} else if (strcmp(orientation, "Right Side Up") == 0) {
		tr = WL_OUTPUT_TRANSFORM_270;
	} else {
		wlr_drm_conn_log(conn, WLR_ERROR, "Unknown panel orientation: %s", orientation);
		tr = WL_OUTPUT_TRANSFORM_NORMAL;
	}

	free(orientation);
	return tr;
}

static const int32_t subpixel_map[] = {
	[DRM_MODE_SUBPIXEL_UNKNOWN] = WL_OUTPUT_SUBPIXEL_UNKNOWN,
	[DRM_MODE_SUBPIXEL_HORIZONTAL_RGB] = WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB,
	[DRM_MODE_SUBPIXEL_HORIZONTAL_BGR] = WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR,
	[DRM_MODE_SUBPIXEL_VERTICAL_RGB] = WL_OUTPUT_SUBPIXEL_VERTICAL_RGB,
	[DRM_MODE_SUBPIXEL_VERTICAL_BGR] = WL_OUTPUT_SUBPIXEL_VERTICAL_BGR,
	[DRM_MODE_SUBPIXEL_NONE] = WL_OUTPUT_SUBPIXEL_NONE,
};

static void dealloc_crtc(struct wlr_drm_connector *conn) {
	if (conn->crtc == NULL) {
		return;
	}

	wlr_drm_conn_log(conn, WLR_DEBUG, "De-allocating CRTC %" PRIu32,
		conn->crtc->id);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, false);
	if (!drm_connector_commit_state(conn, &state, false)) {
		// On GPU unplug, disabling the CRTC can fail with EPERM
		wlr_drm_conn_log(conn, WLR_ERROR, "Failed to disable CRTC %"PRIu32,
			conn->crtc->id);
	}
	wlr_output_state_finish(&state);
}

static void format_nullable_crtc(char *str, size_t size, struct wlr_drm_crtc *crtc) {
	if (crtc != NULL) {
		snprintf(str, size, "CRTC %"PRIu32, crtc->id);
	} else {
		snprintf(str, size, "no CRTC");
	}
}

static void realloc_crtcs(struct wlr_drm_backend *drm,
		struct wlr_drm_connector *want_conn) {
	assert(drm->num_crtcs > 0);

	size_t num_connectors = wl_list_length(&drm->connectors);
	if (num_connectors == 0) {
		return;
	}

	wlr_log(WLR_DEBUG, "Reallocating CRTCs");

	struct wlr_drm_connector *connectors[num_connectors];
	uint32_t connector_constraints[num_connectors];
	uint32_t previous_match[drm->num_crtcs];
	uint32_t new_match[drm->num_crtcs];

	for (size_t i = 0; i < drm->num_crtcs; ++i) {
		previous_match[i] = UNMATCHED;
	}

	size_t i = 0;
	struct wlr_drm_connector *conn;
	wl_list_for_each(conn, &drm->connectors, link) {
		connectors[i] = conn;

		if (conn->crtc) {
			previous_match[conn->crtc - drm->crtcs] = i;
		}

		// Only request a CRTC if the connected is currently enabled or it's the
		// connector the user wants to enable
		bool want_crtc = conn == want_conn || conn->output.enabled;

		if (conn->status == DRM_MODE_CONNECTED && want_crtc) {
			connector_constraints[i] = conn->possible_crtcs;
		} else {
			// Will always fail to match anything
			connector_constraints[i] = 0;
		}

		++i;
	}

	match_connectors_with_crtcs(num_connectors, connector_constraints,
		drm->num_crtcs, previous_match, new_match);

	// Converts our crtc=>connector result into a connector=>crtc one.
	struct wlr_drm_crtc *connector_match[num_connectors];
	for (size_t i = 0 ; i < num_connectors; ++i) {
		connector_match[i] = NULL;
	}
	for (size_t i = 0; i < drm->num_crtcs; ++i) {
		if (new_match[i] != UNMATCHED) {
			connector_match[new_match[i]] = &drm->crtcs[i];
		}
	}

	for (size_t i = 0; i < num_connectors; ++i) {
		struct wlr_drm_connector *conn = connectors[i];
		struct wlr_drm_crtc *new_crtc = connector_match[i];

		char old_crtc_str[16], new_crtc_str[16];
		format_nullable_crtc(old_crtc_str, sizeof(old_crtc_str), conn->crtc);
		format_nullable_crtc(new_crtc_str, sizeof(new_crtc_str), new_crtc);

		char crtc_str[64];
		if (conn->crtc != new_crtc) {
			snprintf(crtc_str, sizeof(crtc_str), "%s → %s", old_crtc_str, new_crtc_str);
		} else {
			snprintf(crtc_str, sizeof(crtc_str), "%s (no change)", new_crtc_str);
		}

		wlr_log(WLR_DEBUG, "  Connector %s (%s%s): %s",
			conn->name, drm_connector_status_str(conn->status),
			connector_constraints[i] != 0 ? ", needs CRTC" : "",
			crtc_str);
	}

	// Refuse to remove a CRTC from an enabled connector, and refuse to
	// change the CRTC of an enabled connector.
	for (size_t i = 0; i < num_connectors; ++i) {
		struct wlr_drm_connector *conn = connectors[i];
		if (conn->status != DRM_MODE_CONNECTED || !conn->output.enabled) {
			continue;
		}
		if (connector_match[i] == NULL) {
			wlr_log(WLR_DEBUG, "Could not match a CRTC for previously connected output; "
				"keeping old configuration");
			return;
		}
		assert(conn->crtc != NULL);
		if (connector_match[i] != conn->crtc) {
			wlr_log(WLR_DEBUG, "Cannot switch CRTC for enabled output; "
				"keeping old configuration");
			return;
		}
	}

	// Apply new configuration
	for (size_t i = 0; i < num_connectors; ++i) {
		struct wlr_drm_connector *conn = connectors[i];

		if (conn->crtc != NULL && connector_match[i]) {
			// We don't need to change anything
			continue;
		}

		dealloc_crtc(conn);
		if (connector_match[i] != NULL) {
			conn->crtc = connector_match[i];
		}
	}
}

static struct wlr_drm_crtc *connector_get_current_crtc(
		struct wlr_drm_connector *wlr_conn, const drmModeConnector *drm_conn) {
	struct wlr_drm_backend *drm = wlr_conn->backend;

	uint32_t crtc_id = 0;
	if (wlr_conn->props.crtc_id != 0) {
		uint64_t value;
		if (!get_drm_prop(drm->fd, wlr_conn->id,
				wlr_conn->props.crtc_id, &value)) {
			wlr_drm_conn_log(wlr_conn, WLR_ERROR,
				"Failed to get CRTC_ID connector property");
			return NULL;
		}
		crtc_id = (uint32_t)value;
	} else if (drm_conn->encoder_id != 0) {
		// Fallback to the legacy API
		drmModeEncoder *enc = drmModeGetEncoder(drm->fd, drm_conn->encoder_id);
		if (enc == NULL) {
			wlr_drm_conn_log(wlr_conn, WLR_ERROR,
				"drmModeGetEncoder() failed");
			return NULL;
		}
		crtc_id = enc->crtc_id;
		drmModeFreeEncoder(enc);
	}
	if (crtc_id == 0) {
		return NULL;
	}

	for (size_t i = 0; i < drm->num_crtcs; ++i) {
		if (drm->crtcs[i].id == crtc_id) {
			return &drm->crtcs[i];
		}
	}

	wlr_drm_conn_log(wlr_conn, WLR_ERROR,
		"Failed to find current CRTC ID %" PRIu32, crtc_id);
	return NULL;
}

static struct wlr_drm_connector *create_drm_connector(struct wlr_drm_backend *drm,
		const drmModeConnector *drm_conn) {
	struct wlr_drm_connector *wlr_conn = calloc(1, sizeof(*wlr_conn));
	if (!wlr_conn) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return NULL;
	}

	wlr_conn->backend = drm;
	wlr_conn->status = DRM_MODE_DISCONNECTED;
	wlr_conn->id = drm_conn->connector_id;

	if (!get_drm_connector_props(drm->fd, wlr_conn->id, &wlr_conn->props)) {
		free(wlr_conn);
		return NULL;
	}

	const char *conn_type_name =
		drmModeGetConnectorTypeName(drm_conn->connector_type);
	if (conn_type_name == NULL) {
		conn_type_name = "Unknown";
	}

	snprintf(wlr_conn->name, sizeof(wlr_conn->name),
		"%s-%"PRIu32, conn_type_name, drm_conn->connector_type_id);

	wlr_conn->possible_crtcs =
		drmModeConnectorGetPossibleCrtcs(drm->fd, drm_conn);
	if (wlr_conn->possible_crtcs == 0) {
		wlr_drm_conn_log(wlr_conn, WLR_ERROR, "No CRTC possible");
	}

	wlr_conn->crtc = connector_get_current_crtc(wlr_conn, drm_conn);

	wl_list_insert(drm->connectors.prev, &wlr_conn->link);
	return wlr_conn;
}

static drmModeModeInfo *connector_get_current_mode(struct wlr_drm_connector *wlr_conn) {
	struct wlr_drm_backend *drm = wlr_conn->backend;

	if (wlr_conn->crtc == NULL) {
		return NULL;
	}

	if (wlr_conn->crtc->props.mode_id != 0) {
		size_t size = 0;
		drmModeModeInfo *mode = get_drm_prop_blob(drm->fd, wlr_conn->crtc->id,
			wlr_conn->crtc->props.mode_id, &size);
		assert(mode == NULL || size == sizeof(*mode));
		return mode;
	} else {
		// Fallback to the legacy API
		drmModeCrtc *drm_crtc = drmModeGetCrtc(drm->fd, wlr_conn->crtc->id);
		if (drm_crtc == NULL) {
			wlr_log_errno(WLR_ERROR, "drmModeGetCrtc failed");
			return NULL;
		}
		if (!drm_crtc->mode_valid) {
			drmModeFreeCrtc(drm_crtc);
			return NULL;
		}
		drmModeModeInfo *mode = malloc(sizeof(*mode));
		if (mode == NULL) {
			wlr_log_errno(WLR_ERROR, "Allocation failed");
			drmModeFreeCrtc(drm_crtc);
			return NULL;
		}
		*mode = drm_crtc->mode;
		drmModeFreeCrtc(drm_crtc);
		return mode;
	}
}

static bool connect_drm_connector(struct wlr_drm_connector *wlr_conn,
		const drmModeConnector *drm_conn) {
	struct wlr_drm_backend *drm = wlr_conn->backend;
	struct wlr_output *output = &wlr_conn->output;

	wlr_log(WLR_DEBUG, "Current CRTC: %d",
		wlr_conn->crtc ? (int)wlr_conn->crtc->id : -1);

	// keep track of all the modes ourselves first. We must only fill out
	// the modes list after wlr_output_init()
	struct wl_list modes;
	wl_list_init(&modes);

	struct wlr_output_state state;
	wlr_output_state_init(&state);
	wlr_output_state_set_enabled(&state, wlr_conn->crtc != NULL);

	drmModeModeInfo *current_modeinfo = connector_get_current_mode(wlr_conn);

	wlr_log(WLR_INFO, "Detected modes:");

	bool found_current_mode = false;
	for (int i = 0; i < drm_conn->count_modes; ++i) {
		if (drm_conn->modes[i].flags & DRM_MODE_FLAG_INTERLACE) {
			continue;
		}

		struct wlr_drm_mode *mode = drm_mode_create(&drm_conn->modes[i]);
		if (!mode) {
			wlr_log_errno(WLR_ERROR, "Allocation failed");
			free(current_modeinfo);
			wlr_output_state_finish(&state);
			return false;
		}

		// If this is the current mode set on the conn's crtc,
		// then set it as the conn's output current mode.
		if (current_modeinfo != NULL && memcmp(&mode->drm_mode,
				current_modeinfo, sizeof(*current_modeinfo)) == 0) {
			wlr_output_state_set_mode(&state, &mode->wlr_mode);
			found_current_mode = true;
		}

		wlr_log(WLR_INFO, "  %"PRId32"x%"PRId32" @ %.3f Hz %s",
			mode->wlr_mode.width, mode->wlr_mode.height,
			(float)mode->wlr_mode.refresh / 1000,
			mode->wlr_mode.preferred ? "(preferred)" : "");

		wl_list_insert(modes.prev, &mode->wlr_mode.link);
	}

	if (current_modeinfo != NULL) {
		int32_t refresh = calculate_refresh_rate(current_modeinfo);

		if (!found_current_mode) {
			wlr_output_state_set_custom_mode(&state,
				current_modeinfo->hdisplay, current_modeinfo->vdisplay, refresh);
		}

		uint64_t mode_id = 0;
		get_drm_prop(drm->fd, wlr_conn->crtc->id,
			wlr_conn->crtc->props.mode_id, &mode_id);

		wlr_conn->crtc->own_mode_id = false;
		wlr_conn->crtc->mode_id = mode_id;
		wlr_conn->refresh = refresh;
	}

	free(current_modeinfo);

	wlr_output_init(output, &drm->backend, &output_impl, drm->session->event_loop, &state);
	wlr_output_state_finish(&state);

	// fill out the modes
	wl_list_insert_list(&output->modes, &modes);

	wlr_output_set_name(output, wlr_conn->name);

	output->phys_width = drm_conn->mmWidth;
	output->phys_height = drm_conn->mmHeight;
	wlr_log(WLR_INFO, "Physical size: %"PRId32"x%"PRId32,
		output->phys_width, output->phys_height);
	if (drm_conn->subpixel < sizeof(subpixel_map) / sizeof(subpixel_map[0])) {
		output->subpixel = subpixel_map[drm_conn->subpixel];
	} else {
		wlr_log(WLR_ERROR, "Unknown subpixel value: %d", (int)drm_conn->subpixel);
	}

	uint64_t non_desktop;
	if (get_drm_prop(drm->fd, wlr_conn->id,
				wlr_conn->props.non_desktop, &non_desktop)) {
		if (non_desktop == 1) {
			wlr_log(WLR_INFO, "Non-desktop connector");
		}
		output->non_desktop = non_desktop;
	}

	memset(wlr_conn->max_bpc_bounds, 0, sizeof(wlr_conn->max_bpc_bounds));
	if (wlr_conn->props.max_bpc != 0) {
		if (!introspect_drm_prop_range(drm->fd, wlr_conn->props.max_bpc,
				&wlr_conn->max_bpc_bounds[0], &wlr_conn->max_bpc_bounds[1])) {
			wlr_log(WLR_ERROR, "Failed to introspect 'max bpc' property");
		}
	}

	uint64_t vrr_capable = 0;
	if (wlr_conn->props.vrr_capable != 0) {
		get_drm_prop(drm->fd, wlr_conn->id, wlr_conn->props.vrr_capable, &vrr_capable);
	}
	output->adaptive_sync_supported = vrr_capable;

	size_t edid_len = 0;
	uint8_t *edid = get_drm_prop_blob(drm->fd,
		wlr_conn->id, wlr_conn->props.edid, &edid_len);
	if (edid_len > 0) {
		parse_edid(wlr_conn, edid_len, edid);
	} else {
		wlr_log(WLR_DEBUG, "Connector has no EDID");
	}
	free(edid);

	char *subconnector = NULL;
	if (wlr_conn->props.subconnector) {
		subconnector = get_drm_prop_enum(drm->fd,
			wlr_conn->id, wlr_conn->props.subconnector);
	}
	if (subconnector && strcmp(subconnector, "Native") == 0) {
		free(subconnector);
		subconnector = NULL;
	}

	char description[128];
	snprintf(description, sizeof(description), "%s %s%s%s (%s%s%s)",
		output->make, output->model,
		output->serial ? " " : "",
		output->serial ? output->serial : "",
		output->name,
		subconnector ? " via " : "",
		subconnector ? subconnector : "");
	wlr_output_set_description(output, description);

	free(subconnector);
	wlr_conn->status = DRM_MODE_CONNECTED;
	return true;
}

static void disconnect_drm_connector(struct wlr_drm_connector *conn);

void scan_drm_connectors(struct wlr_drm_backend *drm,
		struct wlr_device_hotplug_event *event) {
	if (event != NULL && event->connector_id != 0) {
		wlr_log(WLR_INFO, "Scanning DRM connector %"PRIu32" on %s",
			event->connector_id, drm->name);
	} else {
		wlr_log(WLR_INFO, "Scanning DRM connectors on %s", drm->name);
	}

	drmModeRes *res = drmModeGetResources(drm->fd);
	if (!res) {
		wlr_log_errno(WLR_ERROR, "Failed to get DRM resources");
		return;
	}

	size_t seen_len = wl_list_length(&drm->connectors);
	// +1 so length can never be 0, which is undefined behaviour.
	// Last element isn't used.
	bool seen[seen_len + 1];
	memset(seen, false, sizeof(seen));
	size_t new_outputs_len = 0;
	struct wlr_drm_connector *new_outputs[res->count_connectors + 1];

	for (int i = 0; i < res->count_connectors; ++i) {
		uint32_t conn_id = res->connectors[i];

		ssize_t index = -1;
		struct wlr_drm_connector *c, *wlr_conn = NULL;
		wl_list_for_each(c, &drm->connectors, link) {
			index++;
			if (c->id == conn_id) {
				wlr_conn = c;
				break;
			}
		}

		if (wlr_conn && wlr_conn->lease) {
			continue;
		}

		// If the hotplug event contains a connector ID, ignore any other
		// connector.
		if (event != NULL && event->connector_id != 0 &&
				event->connector_id != conn_id) {
			if (wlr_conn != NULL) {
				seen[index] = true;
			}
			continue;
		}

		drmModeConnector *drm_conn = drmModeGetConnector(drm->fd, conn_id);
		if (!drm_conn) {
			wlr_log_errno(WLR_ERROR, "Failed to get DRM connector");
			continue;
		}

		if (!wlr_conn) {
			wlr_conn = create_drm_connector(drm, drm_conn);
			if (wlr_conn == NULL) {
				continue;
			}
			wlr_log(WLR_INFO, "Found connector '%s'", wlr_conn->name);
		} else {
			seen[index] = true;
		}

		// This can only happen *after* hotplug, since we haven't read the
		// connector properties yet
		if (wlr_conn->props.link_status != 0) {
			uint64_t link_status;
			if (!get_drm_prop(drm->fd, wlr_conn->id,
					wlr_conn->props.link_status, &link_status)) {
				wlr_drm_conn_log(wlr_conn, WLR_ERROR,
					"Failed to get link status prop");
				continue;
			}

			if (link_status == DRM_MODE_LINK_STATUS_BAD) {
				// We need to reload our list of modes and force a modeset
				wlr_drm_conn_log(wlr_conn, WLR_INFO, "Bad link detected");
				disconnect_drm_connector(wlr_conn);
			}
		}

		if (wlr_conn->status == DRM_MODE_DISCONNECTED &&
				drm_conn->connection == DRM_MODE_CONNECTED) {
			wlr_log(WLR_INFO, "'%s' connected", wlr_conn->name);
			if (!connect_drm_connector(wlr_conn, drm_conn)) {
				wlr_drm_conn_log(wlr_conn, WLR_ERROR, "Failed to connect DRM connector");
				continue;
			}
			new_outputs[new_outputs_len++] = wlr_conn;
		} else if (wlr_conn->status == DRM_MODE_CONNECTED &&
				drm_conn->connection != DRM_MODE_CONNECTED) {
			wlr_log(WLR_INFO, "'%s' disconnected", wlr_conn->name);
			disconnect_drm_connector(wlr_conn);
		}

		drmModeFreeConnector(drm_conn);
	}

	drmModeFreeResources(res);

	// Iterate in reverse order because we'll remove items from the list and
	// still want indices to remain correct.
	struct wlr_drm_connector *conn, *tmp_conn;
	size_t index = wl_list_length(&drm->connectors);
	wl_list_for_each_reverse_safe(conn, tmp_conn, &drm->connectors, link) {
		index--;
		if (index >= seen_len || seen[index]) {
			continue;
		}

		wlr_log(WLR_INFO, "'%s' disappeared", conn->name);
		destroy_drm_connector(conn);
	}

	for (size_t i = 0; i < new_outputs_len; ++i) {
		struct wlr_drm_connector *conn = new_outputs[i];

		wlr_drm_conn_log(conn, WLR_INFO, "Requesting modeset");
		wl_signal_emit_mutable(&drm->backend.events.new_output,
			&conn->output);
	}
}

void scan_drm_leases(struct wlr_drm_backend *drm) {
	drmModeLesseeListRes *list = drmModeListLessees(drm->fd);
	if (list == NULL) {
		wlr_log_errno(WLR_ERROR, "drmModeListLessees failed");
		return;
	}

	struct wlr_drm_connector *conn;
	wl_list_for_each(conn, &drm->connectors, link) {
		if (conn->lease == NULL) {
			continue;
		}

		bool found = false;
		for (size_t i = 0; i < list->count; i++) {
			if (list->lessees[i] == conn->lease->lessee_id) {
				found = true;
				break;
			}
		}
		if (!found) {
			wlr_log(WLR_DEBUG, "DRM lease %"PRIu32" has been terminated",
				conn->lease->lessee_id);
			drm_lease_destroy(conn->lease);
		}
	}

	drmFree(list);
}

bool commit_drm_device(struct wlr_drm_backend *drm,
		const struct wlr_backend_output_state *output_states, size_t output_states_len,
		bool test_only) {
	if (!drm->session->active) {
		return false;
	}

	struct wlr_drm_connector_state *conn_states = calloc(output_states_len, sizeof(conn_states[0]));
	if (conn_states == NULL) {
		return false;
	}

	bool ok = false;
	bool modeset = false;
	size_t conn_states_len = 0;
	for (size_t i = 0; i < output_states_len; i++) {
		const struct wlr_backend_output_state *output_state = &output_states[i];
		struct wlr_output *output = output_state->output;
		if (!output->enabled && !output_pending_enabled(output, &output_state->base)) {
			// KMS rejects commits which disable already-disabled connectors
			// and have the DRM_MODE_PAGE_FLIP_EVENT flag
			continue;
		}

		struct wlr_drm_connector *conn = get_drm_connector_from_output(output);

		if (output_pending_enabled(output, &output_state->base) && !drm_connector_alloc_crtc(conn)) {
			wlr_drm_conn_log(conn, WLR_DEBUG,
				"No CRTC available for this connector");
			goto out;
		}

		struct wlr_drm_connector_state *conn_state = &conn_states[conn_states_len];
		drm_connector_state_init(conn_state, conn, &output_state->base);
		conn_states_len++;

		if (!drm_connector_prepare(conn_state, test_only)) {
			goto out;
		}

		if (output_state->base.tearing_page_flip) {
			wlr_log(WLR_DEBUG, "Tearing not supported for DRM device-wide commits");
			goto out;
		}

		modeset |= output_state->base.allow_reconfiguration;
	}

	if (test_only && drm->mgpu_renderer.wlr_rend) {
		// If we're running as a secondary GPU, we can't perform an atomic
		// commit without blitting a buffer.
		ok = true;
		goto out;
	}

	uint32_t flags = 0;
	if (!test_only) {
		flags |= DRM_MODE_PAGE_FLIP_EVENT;
	}
	struct wlr_drm_device_state dev_state = {
		.modeset = modeset,
		.connectors = conn_states,
		.connectors_len = conn_states_len,
	};
	ok = drm_commit(drm, &dev_state, flags, test_only);

out:
	for (size_t i = 0; i < conn_states_len; i++) {
		drm_connector_state_finish(&conn_states[i]);
	}
	free(conn_states);
	return ok;
}

static int mhz_to_nsec(int mhz) {
	return 1000000000000LL / mhz;
}

static void handle_page_flip(int fd, unsigned seq,
		unsigned tv_sec, unsigned tv_usec, unsigned crtc_id, void *data) {
	struct wlr_drm_page_flip *page_flip = data;

	struct wlr_drm_connector *conn = drm_page_flip_pop(page_flip, crtc_id);
	if (conn != NULL) {
		conn->pending_page_flip = NULL;
	}

	uint32_t present_flags = WLR_OUTPUT_PRESENT_HW_CLOCK | WLR_OUTPUT_PRESENT_HW_COMPLETION;
	if (!page_flip->async) {
		present_flags |= WLR_OUTPUT_PRESENT_VSYNC;
	}

	if (page_flip->connectors_len == 0) {
		drm_page_flip_destroy(page_flip);
	}

	if (conn == NULL) {
		return;
	}

	struct wlr_drm_backend *drm = conn->backend;

	if (conn->status != DRM_MODE_CONNECTED || conn->crtc == NULL) {
		wlr_drm_conn_log(conn, WLR_DEBUG,
			"Ignoring page-flip event for disabled connector");
		return;
	}

	struct wlr_drm_plane *plane = conn->crtc->primary;
	if (plane->queued_fb) {
		drm_fb_move(&plane->current_fb, &plane->queued_fb);
	}
	if (conn->crtc->cursor && conn->crtc->cursor->queued_fb) {
		drm_fb_move(&conn->crtc->cursor->current_fb,
			&conn->crtc->cursor->queued_fb);
	}

	struct wlr_drm_layer *layer;
	wl_list_for_each(layer, &conn->crtc->layers, link) {
		drm_fb_move(&layer->current_fb, &layer->queued_fb);
	}

	/* Don't report ZERO_COPY in multi-gpu situations, because we had to copy
	 * data between the GPUs, even if we were using the direct scanout
	 * interface.
	 */
	if (!drm->mgpu_renderer.wlr_rend) {
		present_flags |= WLR_OUTPUT_PRESENT_ZERO_COPY;
	}

	struct wlr_output_event_present present_event = {
		/* The DRM backend guarantees that the presentation event will be for
		 * the last submitted frame. */
		.commit_seq = conn->output.commit_seq,
		.presented = drm->session->active,
		.when = {
			.tv_sec = tv_sec,
			.tv_nsec = tv_usec * 1000,
		},
		.seq = seq,
		.refresh = mhz_to_nsec(conn->refresh),
		.flags = present_flags,
	};
	wlr_output_send_present(&conn->output, &present_event);

	if (drm->session->active) {
		wlr_output_send_frame(&conn->output);
	}
}

int handle_drm_event(int fd, uint32_t mask, void *data) {
	struct wlr_drm_backend *drm = data;

	drmEventContext event = {
		.version = 3,
		.page_flip_handler2 = handle_page_flip,
	};

	if (drmHandleEvent(fd, &event) != 0) {
		wlr_log(WLR_ERROR, "drmHandleEvent failed");
		wlr_backend_destroy(&drm->backend);
	}
	return 1;
}

static void disconnect_drm_connector(struct wlr_drm_connector *conn) {
	if (conn->status == DRM_MODE_DISCONNECTED) {
		return;
	}

	// This will cleanup the compositor-facing wlr_output, but won't destroy
	// our wlr_drm_connector.
	wlr_output_destroy(&conn->output);

	assert(conn->status == DRM_MODE_DISCONNECTED);
}

void destroy_drm_connector(struct wlr_drm_connector *conn) {
	disconnect_drm_connector(conn);

	wl_list_remove(&conn->link);
	free(conn);
}

int wlr_drm_backend_get_non_master_fd(struct wlr_backend *backend) {
	assert(backend);

	struct wlr_drm_backend *drm = get_drm_backend_from_backend(backend);
	int fd = open(drm->name, O_RDWR | O_CLOEXEC);

	if (fd < 0) {
		wlr_log_errno(WLR_ERROR, "Unable to clone DRM fd for client fd");
		return -1;
	}

	if (drmIsMaster(fd) && drmDropMaster(fd) < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to drop master");
		return -1;
	}

	return fd;
}

struct wlr_drm_lease *wlr_drm_create_lease(struct wlr_output **outputs,
		size_t n_outputs, int *lease_fd_ptr) {
	assert(outputs);

	if (n_outputs == 0) {
		wlr_log(WLR_ERROR, "Can't lease 0 outputs");
		return NULL;
	}

	struct wlr_drm_backend *drm =
			get_drm_backend_from_backend(outputs[0]->backend);

	int n_objects = 0;
	uint32_t objects[4 * n_outputs + 1];
	for (size_t i = 0; i < n_outputs; ++i) {
		struct wlr_drm_connector *conn =
				get_drm_connector_from_output(outputs[i]);
		assert(conn->lease == NULL);

		if (conn->backend != drm) {
			wlr_log(WLR_ERROR, "Can't lease output from different backends");
			return NULL;
		}

		objects[n_objects++] = conn->id;
		wlr_log(WLR_DEBUG, "Connector %d", conn->id);

		if (!drm_connector_alloc_crtc(conn)) {
			wlr_log(WLR_ERROR, "Failled to allocate connector CRTC");
			return NULL;
		}

		objects[n_objects++] = conn->crtc->id;
		wlr_log(WLR_DEBUG, "CRTC %d", conn->crtc->id);

		objects[n_objects++] = conn->crtc->primary->id;
		wlr_log(WLR_DEBUG, "Primary plane %d", conn->crtc->primary->id);

		if (conn->crtc->cursor) {
			wlr_log(WLR_DEBUG, "Cursor plane %d", conn->crtc->cursor->id);
			objects[n_objects++] = conn->crtc->cursor->id;
		}
	}

	assert(n_objects != 0);

	struct wlr_drm_lease *lease = calloc(1, sizeof(*lease));
	if (lease == NULL) {
		return NULL;
	}

	lease->backend = drm;
	wl_signal_init(&lease->events.destroy);

	wlr_log(WLR_DEBUG, "Issuing DRM lease with %d objects", n_objects);
	int lease_fd = drmModeCreateLease(drm->fd, objects, n_objects, O_CLOEXEC,
			&lease->lessee_id);
	if (lease_fd < 0) {
		free(lease);
		return NULL;
	}
	*lease_fd_ptr = lease_fd;

	wlr_log(WLR_DEBUG, "Issued DRM lease %"PRIu32, lease->lessee_id);
	for (size_t i = 0; i < n_outputs; ++i) {
		struct wlr_drm_connector *conn =
				get_drm_connector_from_output(outputs[i]);
		conn->lease = lease;
		conn->crtc->lease = lease;
		disconnect_drm_connector(conn);
	}

	return lease;
}

void wlr_drm_lease_terminate(struct wlr_drm_lease *lease) {
	struct wlr_drm_backend *drm = lease->backend;

	wlr_log(WLR_DEBUG, "Terminating DRM lease %d", lease->lessee_id);
	int ret = drmModeRevokeLease(drm->fd, lease->lessee_id);
	if (ret < 0) {
		wlr_log_errno(WLR_ERROR, "Failed to terminate lease");
	}

	drm_lease_destroy(lease);
}

void drm_lease_destroy(struct wlr_drm_lease *lease) {
	struct wlr_drm_backend *drm = lease->backend;

	wl_signal_emit_mutable(&lease->events.destroy, NULL);

	assert(wl_list_empty(&lease->events.destroy.listener_list));

	struct wlr_drm_connector *conn;
	wl_list_for_each(conn, &drm->connectors, link) {
		if (conn->lease == lease) {
			conn->lease = NULL;
		}
	}

	for (size_t i = 0; i < drm->num_crtcs; ++i) {
		if (drm->crtcs[i].lease == lease) {
			drm->crtcs[i].lease = NULL;
		}
	}

	free(lease);
	scan_drm_connectors(drm, NULL);
}
