#include <drm_fourcc.h>
#include <stdlib.h>
#include <wlr/render/drm_format_set.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/util/addon.h>
#include <wlr/util/log.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include "backend/drm/drm.h"
#include "backend/drm/fb.h"
#include "render/pixel_format.h"

void drm_fb_clear(struct wlr_drm_fb **fb_ptr) {
	if (*fb_ptr == NULL) {
		return;
	}

	struct wlr_drm_fb *fb = *fb_ptr;
	wlr_buffer_unlock(fb->wlr_buf); // may destroy the buffer

	*fb_ptr = NULL;
}

struct wlr_drm_fb *drm_fb_lock(struct wlr_drm_fb *fb) {
	wlr_buffer_lock(fb->wlr_buf);
	return fb;
}

static void drm_fb_handle_destroy(struct wlr_addon *addon) {
	struct wlr_drm_fb *fb = wl_container_of(addon, fb, addon);
	drm_fb_destroy(fb);
}

static const struct wlr_addon_interface fb_addon_impl = {
	.name = "wlr_drm_fb",
	.destroy = drm_fb_handle_destroy,
};

static uint32_t get_fb_for_bo(struct wlr_drm_backend *drm,
		struct wlr_dmabuf_attributes *dmabuf, uint32_t handles[static 4]) {
	uint64_t modifiers[4] = {0};
	for (int i = 0; i < dmabuf->n_planes; i++) {
		// KMS requires all BO planes to have the same modifier
		modifiers[i] = dmabuf->modifier;
	}

	uint32_t id = 0;
	if (drm->addfb2_modifiers && dmabuf->modifier != DRM_FORMAT_MOD_INVALID) {
		if (drmModeAddFB2WithModifiers(drm->fd, dmabuf->width, dmabuf->height,
				dmabuf->format, handles, dmabuf->stride, dmabuf->offset,
				modifiers, &id, DRM_MODE_FB_MODIFIERS) != 0) {
			wlr_log_errno(WLR_DEBUG, "drmModeAddFB2WithModifiers failed");
		}
	} else {
		if (dmabuf->modifier != DRM_FORMAT_MOD_INVALID &&
				dmabuf->modifier != DRM_FORMAT_MOD_LINEAR) {
			wlr_log(WLR_ERROR, "Cannot import DRM framebuffer with explicit "
				"modifier 0x%"PRIX64, dmabuf->modifier);
			return 0;
		}

		int ret = drmModeAddFB2(drm->fd, dmabuf->width, dmabuf->height,
			dmabuf->format, handles, dmabuf->stride, dmabuf->offset, &id, 0);
		if (ret != 0 && dmabuf->format == DRM_FORMAT_ARGB8888 &&
				dmabuf->n_planes == 1 && dmabuf->offset[0] == 0) {
			// Some big-endian machines don't support drmModeAddFB2. Try a
			// last-resort fallback for ARGB8888 buffers, like Xorg's
			// modesetting driver does.
			wlr_log(WLR_DEBUG, "drmModeAddFB2 failed (%s), falling back to "
				"legacy drmModeAddFB", strerror(-ret));

			uint32_t depth = 32;
			uint32_t bpp = 32;
			ret = drmModeAddFB(drm->fd, dmabuf->width, dmabuf->height, depth,
				bpp, dmabuf->stride[0], handles[0], &id);
			if (ret != 0) {
				wlr_log_errno(WLR_DEBUG, "drmModeAddFB failed");
			}
		} else if (ret != 0) {
			wlr_log_errno(WLR_DEBUG, "drmModeAddFB2 failed");
		}
	}

	return id;
}

static void close_all_bo_handles(struct wlr_drm_backend *drm,
		uint32_t handles[static 4]) {
	for (int i = 0; i < 4; ++i) {
		if (handles[i] == 0) {
			continue;
		}

		// If multiple planes share the same BO handle, avoid double-closing it
		bool already_closed = false;
		for (int j = 0; j < i; ++j) {
			if (handles[i] == handles[j]) {
				already_closed = true;
				break;
			}
		}
		if (already_closed) {
			continue;
		}

		if (drmCloseBufferHandle(drm->fd, handles[i]) != 0) {
			wlr_log_errno(WLR_ERROR, "drmCloseBufferHandle failed");
		}
	}
}

static void drm_poisoned_fb_handle_destroy(struct wlr_addon *addon) {
	wlr_addon_finish(addon);
	free(addon);
}

static const struct wlr_addon_interface poisoned_fb_addon_impl = {
	.name = "wlr_drm_poisoned_fb",
	.destroy = drm_poisoned_fb_handle_destroy,
};

static bool is_buffer_poisoned(struct wlr_drm_backend *drm,
		struct wlr_buffer *buf) {
	return wlr_addon_find(&buf->addons, drm, &poisoned_fb_addon_impl) != NULL;
}

/**
 * Mark the buffer as "poisoned", ie. it cannot be imported into KMS. This
 * allows us to avoid repeatedly trying to import it when it's not
 * scanout-capable.
 */
static void poison_buffer(struct wlr_drm_backend *drm,
		struct wlr_buffer *buf) {
	struct wlr_addon *addon = calloc(1, sizeof(*addon));
	if (addon == NULL) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return;
	}
	wlr_addon_init(addon, &buf->addons, drm, &poisoned_fb_addon_impl);
	wlr_log(WLR_DEBUG, "Poisoning buffer");
}

static struct wlr_drm_fb *drm_fb_create(struct wlr_drm_backend *drm,
		struct wlr_buffer *buf, const struct wlr_drm_format_set *formats) {
	struct wlr_dmabuf_attributes attribs;
	if (!wlr_buffer_get_dmabuf(buf, &attribs)) {
		wlr_log(WLR_DEBUG, "Failed to get DMA-BUF from buffer");
		return NULL;
	}

	if (is_buffer_poisoned(drm, buf)) {
		wlr_log(WLR_DEBUG, "Buffer is poisoned");
		return NULL;
	}

	struct wlr_drm_fb *fb = calloc(1, sizeof(*fb));
	if (!fb) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return NULL;
	}

	if (formats && !wlr_drm_format_set_has(formats, attribs.format,
			attribs.modifier)) {
		// The format isn't supported by the plane. Try stripping the alpha
		// channel, if any.
		const struct wlr_pixel_format_info *info =
			drm_get_pixel_format_info(attribs.format);
		if (info != NULL && info->opaque_substitute != DRM_FORMAT_INVALID &&
				wlr_drm_format_set_has(formats, info->opaque_substitute, attribs.modifier)) {
			attribs.format = info->opaque_substitute;
		} else {
			wlr_log(WLR_DEBUG, "Buffer format 0x%"PRIX32" with modifier "
				"0x%"PRIX64" cannot be scanned out",
				attribs.format, attribs.modifier);
			goto error_fb;
		}
	}

	uint32_t handles[4] = {0};
	for (int i = 0; i < attribs.n_planes; ++i) {
		int ret = drmPrimeFDToHandle(drm->fd, attribs.fd[i], &handles[i]);
		if (ret != 0) {
			wlr_log_errno(WLR_DEBUG, "drmPrimeFDToHandle failed");
			goto error_bo_handle;
		}
	}

	fb->id = get_fb_for_bo(drm, &attribs, handles);
	if (!fb->id) {
		wlr_log(WLR_DEBUG, "Failed to import BO in KMS");
		poison_buffer(drm, buf);
		goto error_bo_handle;
	}

	close_all_bo_handles(drm, handles);

	fb->backend = drm;
	fb->wlr_buf = buf;

	wlr_addon_init(&fb->addon, &buf->addons, drm, &fb_addon_impl);
	wl_list_insert(&drm->fbs, &fb->link);

	return fb;

error_bo_handle:
	close_all_bo_handles(drm, handles);
error_fb:
	free(fb);
	return NULL;
}

void drm_fb_destroy(struct wlr_drm_fb *fb) {
	struct wlr_drm_backend *drm = fb->backend;

	wl_list_remove(&fb->link);
	wlr_addon_finish(&fb->addon);

	int ret = drmModeCloseFB(drm->fd, fb->id);
	if (ret == -EINVAL) {
		ret = drmModeRmFB(drm->fd, fb->id);
	}
	if (ret != 0) {
		wlr_log(WLR_ERROR, "Failed to close FB: %s", strerror(-ret));
	}

	free(fb);
}

bool drm_fb_import(struct wlr_drm_fb **fb_ptr, struct wlr_drm_backend *drm,
		struct wlr_buffer *buf, const struct wlr_drm_format_set *formats) {
	struct wlr_drm_fb *fb;
	struct wlr_addon *addon = wlr_addon_find(&buf->addons, drm, &fb_addon_impl);
	if (addon != NULL) {
		fb = wl_container_of(addon, fb, addon);
	} else {
		fb = drm_fb_create(drm, buf, formats);
		if (!fb) {
			return false;
		}
	}

	wlr_buffer_lock(buf);
	drm_fb_move(fb_ptr, &fb);
	return true;
}

void drm_fb_move(struct wlr_drm_fb **new, struct wlr_drm_fb **old) {
	drm_fb_clear(new);
	*new = *old;
	*old = NULL;
}

void drm_fb_copy(struct wlr_drm_fb **new, struct wlr_drm_fb *old) {
	drm_fb_clear(new);
	if (old != NULL) {
		*new = drm_fb_lock(old);
	}
}
