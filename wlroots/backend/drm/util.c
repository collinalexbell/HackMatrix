#include <assert.h>
#include <drm_fourcc.h>
#include <drm_mode.h>
#include <drm.h>
#include <libdisplay-info/cvt.h>
#include <libdisplay-info/edid.h>
#include <libdisplay-info/info.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wlr/util/log.h>
#include "backend/drm/drm.h"
#include "backend/drm/util.h"

int32_t calculate_refresh_rate(const drmModeModeInfo *mode) {
	int32_t refresh = (mode->clock * 1000000LL / mode->htotal +
		mode->vtotal / 2) / mode->vtotal;

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		refresh *= 2;
	}

	if (mode->flags & DRM_MODE_FLAG_DBLSCAN) {
		refresh /= 2;
	}

	if (mode->vscan > 1) {
		refresh /= mode->vscan;
	}

	return refresh;
}

enum wlr_output_mode_aspect_ratio get_picture_aspect_ratio(const drmModeModeInfo *mode) {
	switch (mode->flags & DRM_MODE_FLAG_PIC_AR_MASK) {
	case DRM_MODE_FLAG_PIC_AR_NONE:
		return WLR_OUTPUT_MODE_ASPECT_RATIO_NONE;
	case DRM_MODE_FLAG_PIC_AR_4_3:
		return WLR_OUTPUT_MODE_ASPECT_RATIO_4_3;
	case DRM_MODE_FLAG_PIC_AR_16_9:
		return WLR_OUTPUT_MODE_ASPECT_RATIO_16_9;
	case DRM_MODE_FLAG_PIC_AR_64_27:
		return WLR_OUTPUT_MODE_ASPECT_RATIO_64_27;
	case DRM_MODE_FLAG_PIC_AR_256_135:
		return WLR_OUTPUT_MODE_ASPECT_RATIO_256_135;
	default:
		wlr_log(WLR_ERROR, "Unknown mode picture aspect ratio: %u",
			mode->flags & DRM_MODE_FLAG_PIC_AR_MASK);
		return WLR_OUTPUT_MODE_ASPECT_RATIO_NONE;
	}
}

void parse_edid(struct wlr_drm_connector *conn, size_t len, const uint8_t *data) {
	struct wlr_output *output = &conn->output;

	free(output->make);
	free(output->model);
	free(output->serial);
	output->make = NULL;
	output->model = NULL;
	output->serial = NULL;

	struct di_info *info = di_info_parse_edid(data, len);
	if (info == NULL) {
		wlr_log(WLR_ERROR, "Failed to parse EDID");
		return;
	}

	const struct di_edid *edid = di_info_get_edid(info);
	const struct di_edid_vendor_product *vendor_product = di_edid_get_vendor_product(edid);
	char pnp_id[] = {
		vendor_product->manufacturer[0],
		vendor_product->manufacturer[1],
		vendor_product->manufacturer[2],
		'\0',
	};
	const char *manu = get_pnp_manufacturer(vendor_product->manufacturer);
	if (!manu) {
		manu = pnp_id;
	}
	output->make = strdup(manu);

	output->model = di_info_get_model(info);
	output->serial = di_info_get_serial(info);

	const struct di_color_primaries *color_characteristics = di_info_get_default_color_primaries(info);
	if (color_characteristics->has_primaries) {
		output->default_primaries_value = (struct wlr_color_primaries) {
			.red = { .x = color_characteristics->primary[0].x, .y = color_characteristics->primary[0].y },
			.green = { .x = color_characteristics->primary[1].x, .y = color_characteristics->primary[1].y },
			.blue = { .x = color_characteristics->primary[2].x, .y = color_characteristics->primary[2].y },
			.white = { .x = color_characteristics->default_white.x, .y = color_characteristics->default_white.y },
		};
		output->default_primaries = &output->default_primaries_value;
	}

	const struct di_supported_signal_colorimetry *colorimetry = di_info_get_supported_signal_colorimetry(info);
	bool has_bt2020 = colorimetry->bt2020_cycc || colorimetry->bt2020_ycc || colorimetry->bt2020_rgb;
	if (conn->props.colorspace != 0 && has_bt2020) {
		output->supported_primaries |= WLR_COLOR_NAMED_PRIMARIES_BT2020;
	}

	const struct di_hdr_static_metadata *hdr_static_metadata = di_info_get_hdr_static_metadata(info);
	if (conn->props.hdr_output_metadata != 0 && hdr_static_metadata->type1 && hdr_static_metadata->pq) {
		output->supported_transfer_functions |= WLR_COLOR_TRANSFER_FUNCTION_ST2084_PQ;
	}

	di_info_destroy(info);
}

const char *drm_connector_status_str(drmModeConnection status) {
	switch (status) {
	case DRM_MODE_CONNECTED:
		return "connected";
	case DRM_MODE_DISCONNECTED:
		return "disconnected";
	case DRM_MODE_UNKNOWNCONNECTION:
		return "unknown";
	}
	return "<unsupported>";
}

static bool is_taken(size_t n, const uint32_t arr[static n], uint32_t key) {
	for (size_t i = 0; i < n; ++i) {
		if (arr[i] == key) {
			return true;
		}
	}
	return false;
}

/*
 * Store all of the non-recursive state in a struct, so we aren't literally
 * passing 12 arguments to a function.
 */
struct match_state {
	const size_t num_conns;
	const uint32_t *restrict conns;
	const size_t num_crtcs;
	size_t score;
	size_t replaced;
	uint32_t *restrict res;
	uint32_t *restrict best;
	const uint32_t *restrict orig;
	bool exit_early;
};

/**
 * Step to process a CRTC.
 *
 * This is a naive implementation of maximum bipartite matching.
 *
 * score: The number of connectors we've matched so far.
 * replaced: The number of changes from the original solution.
 * crtc_index: The index of the current CRTC.
 *
 * This tries to match a solution as close to st->orig as it can.
 *
 * Returns whether we've set a new best element with this solution.
 */
static bool match_connectors_with_crtcs_(struct match_state *st,
		size_t score, size_t replaced, size_t crtc_index) {
	// Finished
	if (crtc_index >= st->num_crtcs) {
		if (score > st->score ||
				(score == st->score && replaced < st->replaced)) {
			st->score = score;
			st->replaced = replaced;
			memcpy(st->best, st->res, sizeof(st->best[0]) * st->num_crtcs);

			st->exit_early = (st->score == st->num_crtcs
					|| st->score == st->num_conns)
					&& st->replaced == 0;

			return true;
		} else {
			return false;
		}
	}

	bool has_best = false;

	/*
	 * Attempt to use the current solution first, to try and avoid
	 * recalculating everything
	 */
	if (st->orig[crtc_index] != UNMATCHED && !is_taken(crtc_index, st->res, st->orig[crtc_index])) {
		st->res[crtc_index] = st->orig[crtc_index];
		size_t crtc_score = st->conns[st->res[crtc_index]] != 0 ? 1 : 0;
		if (match_connectors_with_crtcs_(st, score + crtc_score, replaced, crtc_index + 1)) {
			has_best = true;
		}
	}
	if (st->exit_early) {
		return true;
	}

	if (st->orig[crtc_index] != UNMATCHED) {
		++replaced;
	}

	for (size_t candidate = 0; candidate < st->num_conns; ++candidate) {
		// We tried this earlier
		if (candidate == st->orig[crtc_index]) {
			continue;
		}

		// Not compatible
		if (!(st->conns[candidate] & (1 << crtc_index))) {
			continue;
		}

		// Already taken
		if (is_taken(crtc_index, st->res, candidate)) {
			continue;
		}

		st->res[crtc_index] = candidate;
		size_t crtc_score = st->conns[candidate] != 0 ? 1 : 0;
		if (match_connectors_with_crtcs_(st, score + crtc_score, replaced, crtc_index + 1)) {
			has_best = true;
		}

		if (st->exit_early) {
			return true;
		}
	}

	// Maybe this CRTC can't be matched
	st->res[crtc_index] = UNMATCHED;
	if (match_connectors_with_crtcs_(st, score, replaced, crtc_index + 1)) {
		has_best = true;
	}

	return has_best;
}

void match_connectors_with_crtcs(size_t num_conns,
		const uint32_t conns[static restrict num_conns],
		size_t num_crtcs, const uint32_t prev_crtcs[static restrict num_crtcs],
		uint32_t new_crtcs[static restrict num_crtcs]) {
	uint32_t solution[num_crtcs];
	for (size_t i = 0; i < num_crtcs; ++i) {
		solution[i] = UNMATCHED;
	}

	struct match_state st = {
		.num_conns = num_conns,
		.num_crtcs = num_crtcs,
		.score = 0,
		.replaced = SIZE_MAX,
		.conns = conns,
		.res = solution,
		.best = new_crtcs,
		.orig = prev_crtcs,
		.exit_early = false,
	};

	match_connectors_with_crtcs_(&st, 0, 0, 0);
}

void generate_cvt_mode(drmModeModeInfo *mode, int hdisplay, int vdisplay,
		float vrefresh) {
	// TODO: depending on capabilities advertised in the EDID, use reduced
	// blanking if possible (and update sync polarity)
	struct di_cvt_options options = {
		.red_blank_ver = DI_CVT_REDUCED_BLANKING_NONE,
		.h_pixels = hdisplay,
		.v_lines = vdisplay,
		.ip_freq_rqd = vrefresh ? vrefresh : 60,
	};
	struct di_cvt_timing timing;
	di_cvt_compute(&timing, &options);

	uint16_t hsync_start = hdisplay + timing.h_front_porch;
	uint16_t vsync_start = timing.v_lines_rnd + timing.v_front_porch;
	uint16_t hsync_end = hsync_start + timing.h_sync;
	uint16_t vsync_end = vsync_start + timing.v_sync;

	*mode = (drmModeModeInfo){
		.clock = roundf(timing.act_pixel_freq * 1000),
		.hdisplay = hdisplay,
		.vdisplay = timing.v_lines_rnd,
		.hsync_start = hsync_start,
		.vsync_start = vsync_start,
		.hsync_end = hsync_end,
		.vsync_end = vsync_end,
		.htotal = hsync_end + timing.h_back_porch,
		.vtotal = vsync_end + timing.v_back_porch,
		.vrefresh = roundf(timing.act_frame_rate),
		.flags = DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_PVSYNC,
	};
	snprintf(mode->name, sizeof(mode->name), "%dx%d", hdisplay, vdisplay);
}
