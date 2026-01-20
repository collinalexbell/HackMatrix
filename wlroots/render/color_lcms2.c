#include <assert.h>
#include <lcms2.h>
#include <stdlib.h>
#include <wlr/util/log.h>
#include <wlr/render/color.h>
#include "render/color.h"

struct wlr_color_transform_lcms2 {
	struct wlr_color_transform base;

	cmsContext ctx;
	cmsHTRANSFORM lcms;
};

static const cmsCIExyY srgb_whitepoint = { 0.3127, 0.3291, 1 };

static const cmsCIExyYTRIPLE srgb_primaries = {
	.Red = { 0.64, 0.33, 1 },
	.Green = { 0.3, 0.6, 1 },
	.Blue = { 0.15, 0.06, 1},
};

static void handle_lcms_error(cmsContext ctx, cmsUInt32Number code, const char *text) {
	wlr_log(WLR_ERROR, "[lcms] %s", text);
}

struct wlr_color_transform *wlr_color_transform_init_linear_to_icc(
		const void *data, size_t size) {
	struct wlr_color_transform_lcms2 *tx = NULL;

	cmsContext ctx = cmsCreateContext(NULL, NULL);
	if (ctx == NULL) {
		wlr_log(WLR_ERROR, "cmsCreateContext failed");
		return NULL;
	}

	cmsSetLogErrorHandlerTHR(ctx, handle_lcms_error);

	cmsHPROFILE icc_profile = cmsOpenProfileFromMemTHR(ctx, data, size);
	if (icc_profile == NULL) {
		wlr_log(WLR_ERROR, "cmsOpenProfileFromMemTHR failed");
		goto error_ctx;
	}

	if (cmsGetDeviceClass(icc_profile) != cmsSigDisplayClass) {
		wlr_log(WLR_ERROR, "ICC profile must have the Display device class");
		goto error_icc_profile;
	}

	cmsToneCurve *linear_tone_curve = cmsBuildGamma(ctx, 1);
	if (linear_tone_curve == NULL) {
		wlr_log(WLR_ERROR, "cmsBuildGamma failed");
		goto error_icc_profile;
	}

	cmsToneCurve *linear_tf[] = {
		linear_tone_curve,
		linear_tone_curve,
		linear_tone_curve,
	};
	cmsHPROFILE srgb_profile = cmsCreateRGBProfileTHR(ctx, &srgb_whitepoint,
		&srgb_primaries, linear_tf);
	cmsFreeToneCurve(linear_tone_curve);
	if (srgb_profile == NULL) {
		wlr_log(WLR_ERROR, "cmsCreateRGBProfileTHR failed");
		goto error_icc_profile;
	}

	cmsHTRANSFORM lcms_tr = cmsCreateTransformTHR(ctx,
		srgb_profile, TYPE_RGB_FLT, icc_profile, TYPE_RGB_FLT,
		INTENT_RELATIVE_COLORIMETRIC, 0);
	cmsCloseProfile(srgb_profile);
	cmsCloseProfile(icc_profile);
	if (lcms_tr == NULL) {
		wlr_log(WLR_ERROR, "cmsCreateTransformTHR failed");
		goto error_ctx;
	}

	tx = calloc(1, sizeof(*tx));
	if (!tx) {
		cmsDeleteTransform(lcms_tr);
		goto error_ctx;
	}
	wlr_color_transform_init(&tx->base, COLOR_TRANSFORM_LCMS2);

	tx->ctx = ctx;
	tx->lcms = lcms_tr;

	return &tx->base;

error_icc_profile:
	cmsCloseProfile(icc_profile);
error_ctx:
	cmsDeleteContext(ctx);
	return NULL;
}

void color_transform_lcms2_finish(struct wlr_color_transform_lcms2 *tr) {
	cmsDeleteTransform(tr->lcms);
	cmsDeleteContext(tr->ctx);
}

struct wlr_color_transform_lcms2 *color_transform_lcms2_from_base(
		struct wlr_color_transform *tr) {
	assert(tr->type == COLOR_TRANSFORM_LCMS2);
	struct wlr_color_transform_lcms2 *lcms2 = wl_container_of(tr, lcms2, base);
	return lcms2;
}

void color_transform_lcms2_eval(struct wlr_color_transform_lcms2 *tr,
		float out[static 3], const float in[static 3]) {
	cmsDoTransform(tr->lcms, in, out, 1);
}
