#include <assert.h>
#include <drm_fourcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-server-core.h>
#include <wlr/backend/interface.h>
#include <wlr/backend/session.h>
#include <wlr/interfaces/wlr_output.h>
#include <wlr/util/log.h>
#include <xf86drm.h>
#include "backend/drm/drm.h"
#include "backend/drm/fb.h"
#include "render/drm_format_set.h"

struct wlr_drm_backend *get_drm_backend_from_backend(
		struct wlr_backend *wlr_backend) {
	assert(wlr_backend_is_drm(wlr_backend));
	struct wlr_drm_backend *backend = wl_container_of(wlr_backend, backend, backend);
	return backend;
}

static bool backend_start(struct wlr_backend *backend) {
	struct wlr_drm_backend *drm = get_drm_backend_from_backend(backend);
	scan_drm_connectors(drm, NULL);
	return true;
}

static void backend_destroy(struct wlr_backend *backend) {
	if (!backend) {
		return;
	}

	struct wlr_drm_backend *drm = get_drm_backend_from_backend(backend);

	struct wlr_drm_connector *conn, *next;
	wl_list_for_each_safe(conn, next, &drm->connectors, link) {
		conn->crtc = NULL; // leave CRTCs on when shutting down
		destroy_drm_connector(conn);
	}

	struct wlr_drm_page_flip *page_flip, *page_flip_tmp;
	wl_list_for_each_safe(page_flip, page_flip_tmp, &drm->page_flips, link) {
		drm_page_flip_destroy(page_flip);
	}

	wlr_backend_finish(backend);

	wl_list_remove(&drm->session_destroy.link);
	wl_list_remove(&drm->session_active.link);
	wl_list_remove(&drm->parent_destroy.link);
	wl_list_remove(&drm->dev_change.link);
	wl_list_remove(&drm->dev_remove.link);

	if (drm->mgpu_renderer.wlr_rend) {
		wlr_drm_format_set_finish(&drm->mgpu_formats);
		finish_drm_renderer(&drm->mgpu_renderer);
	}

	finish_drm_resources(drm);

	struct wlr_drm_fb *fb, *fb_tmp;
	wl_list_for_each_safe(fb, fb_tmp, &drm->fbs, link) {
		drm_fb_destroy(fb);
	}

	free(drm->name);
	wlr_session_close_file(drm->session, drm->dev);
	wl_event_source_remove(drm->drm_event);
	free(drm);
}

static int backend_get_drm_fd(struct wlr_backend *backend) {
	struct wlr_drm_backend *drm = get_drm_backend_from_backend(backend);
	return drm->fd;
}

static bool backend_test(struct wlr_backend *backend,
		const struct wlr_backend_output_state *states, size_t states_len) {
	struct wlr_drm_backend *drm = get_drm_backend_from_backend(backend);
	return commit_drm_device(drm, states, states_len, true);
}

static bool backend_commit(struct wlr_backend *backend,
		const struct wlr_backend_output_state *states, size_t states_len) {
	struct wlr_drm_backend *drm = get_drm_backend_from_backend(backend);
	return commit_drm_device(drm, states, states_len, false);
}

static const struct wlr_backend_impl backend_impl = {
	.start = backend_start,
	.destroy = backend_destroy,
	.get_drm_fd = backend_get_drm_fd,
	.test = backend_test,
	.commit = backend_commit,
};

bool wlr_backend_is_drm(struct wlr_backend *b) {
	return b->impl == &backend_impl;
}

struct wlr_backend *wlr_drm_backend_get_parent(struct wlr_backend *backend) {
	struct wlr_drm_backend *drm = get_drm_backend_from_backend(backend);
	return drm->parent ? &drm->parent->backend : NULL;
}

static void handle_session_active(struct wl_listener *listener, void *data) {
	struct wlr_drm_backend *drm =
		wl_container_of(listener, drm, session_active);
	struct wlr_session *session = drm->session;

	wlr_log(WLR_INFO, "DRM FD %s", session->active ? "resumed" : "paused");

	if (!session->active) {
		// Disconnect any active connectors so that the client will modeset and
		// rerender when the session is activated again.
		struct wlr_drm_connector *conn;
		wl_list_for_each(conn, &drm->connectors, link) {
			if (conn->status == DRM_MODE_CONNECTED) {
				wlr_output_destroy(&conn->output);
			}
		}
		return;
	}

	scan_drm_connectors(drm, NULL);
}

static void handle_dev_change(struct wl_listener *listener, void *data) {
	struct wlr_drm_backend *drm = wl_container_of(listener, drm, dev_change);
	struct wlr_device_change_event *change = data;

	if (!drm->session->active) {
		return;
	}

	switch (change->type) {
	case WLR_DEVICE_HOTPLUG:
		wlr_log(WLR_DEBUG, "Received hotplug event for %s", drm->name);
		scan_drm_connectors(drm, &change->hotplug);
		break;
	case WLR_DEVICE_LEASE:
		wlr_log(WLR_DEBUG, "Received lease event for %s", drm->name);
		scan_drm_leases(drm);
		break;
	default:
		wlr_log(WLR_DEBUG, "Received unknown change event for %s", drm->name);
	}
}

static void handle_dev_remove(struct wl_listener *listener, void *data) {
	struct wlr_drm_backend *drm = wl_container_of(listener, drm, dev_remove);

	wlr_log(WLR_INFO, "Destroying DRM backend for %s", drm->name);
	backend_destroy(&drm->backend);
}

static void handle_session_destroy(struct wl_listener *listener, void *data) {
	struct wlr_drm_backend *drm =
		wl_container_of(listener, drm, session_destroy);
	backend_destroy(&drm->backend);
}

static void handle_parent_destroy(struct wl_listener *listener, void *data) {
	struct wlr_drm_backend *drm =
		wl_container_of(listener, drm, parent_destroy);
	backend_destroy(&drm->backend);
}

static void sanitize_mgpu_modifiers(struct wlr_drm_format_set *set) {
	for (size_t idx = 0; idx < set->len; idx++) {
		// Implicit modifiers are not well-defined across devices, so strip
		// them from all formats in multi-gpu scenarios.
		struct wlr_drm_format *fmt = &set->formats[idx];
		wlr_drm_format_set_remove(set, fmt->format, DRM_FORMAT_MOD_INVALID);
	}
}

static bool init_mgpu_renderer(struct wlr_drm_backend *drm) {
	if (!init_drm_renderer(drm, &drm->mgpu_renderer)) {
		wlr_log(WLR_INFO, "Failed to initialize mgpu blit renderer"
			", falling back to scanning out from primary GPU");

		for (uint32_t plane_idx = 0; plane_idx < drm->num_planes; plane_idx++) {
			struct wlr_drm_plane *plane = &drm->planes[plane_idx];
			sanitize_mgpu_modifiers(&plane->formats);
		}
		return true;
	}

	// We'll perform a multi-GPU copy for all submitted buffers, we need
	// to be able to texture from them
	struct wlr_renderer *renderer = drm->mgpu_renderer.wlr_rend;
	const struct wlr_drm_format_set *texture_formats =
		wlr_renderer_get_texture_formats(renderer, WLR_BUFFER_CAP_DMABUF);
	if (texture_formats == NULL) {
		wlr_log(WLR_ERROR, "Failed to query renderer texture formats");
		return false;
	}

	wlr_drm_format_set_copy(&drm->mgpu_formats, texture_formats);
	sanitize_mgpu_modifiers(&drm->mgpu_formats);
	drm->backend.features.timeline = drm->backend.features.timeline &&
		drm->mgpu_renderer.wlr_rend->features.timeline;
	return true;
}

struct wlr_backend *wlr_drm_backend_create(struct wlr_session *session,
		struct wlr_device *dev, struct wlr_backend *parent) {
	assert(session && dev);
	assert(!parent || wlr_backend_is_drm(parent));

	char *name = drmGetDeviceNameFromFd2(dev->fd);
	if (name == NULL) {
		wlr_log_errno(WLR_ERROR, "drmGetDeviceNameFromFd2() failed");
		return NULL;
	}

	drmVersion *version = drmGetVersion(dev->fd);
	if (version == NULL) {
		wlr_log_errno(WLR_ERROR, "drmGetVersion() failed");
		free(name);
		return NULL;
	}
	wlr_log(WLR_INFO, "Initializing DRM backend for %s (%s)", name, version->name);
	drmFreeVersion(version);

	struct wlr_drm_backend *drm = calloc(1, sizeof(*drm));
	if (!drm) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return NULL;
	}
	wlr_backend_init(&drm->backend, &backend_impl);

	drm->backend.buffer_caps = WLR_BUFFER_CAP_DMABUF;

	drm->session = session;
	wl_list_init(&drm->fbs);
	wl_list_init(&drm->connectors);
	wl_list_init(&drm->page_flips);

	drm->dev = dev;
	drm->fd = dev->fd;
	drm->name = name;

	if (parent != NULL) {
		drm->parent = get_drm_backend_from_backend(parent);

		drm->parent_destroy.notify = handle_parent_destroy;
		wl_signal_add(&parent->events.destroy, &drm->parent_destroy);
	} else {
		wl_list_init(&drm->parent_destroy.link);
	}

	drm->dev_change.notify = handle_dev_change;
	wl_signal_add(&dev->events.change, &drm->dev_change);

	drm->dev_remove.notify = handle_dev_remove;
	wl_signal_add(&dev->events.remove, &drm->dev_remove);

	drm->drm_event = wl_event_loop_add_fd(session->event_loop, drm->fd,
		WL_EVENT_READABLE, handle_drm_event, drm);
	if (!drm->drm_event) {
		wlr_log(WLR_ERROR, "Failed to create DRM event source");
		goto error_fd;
	}

	drm->session_active.notify = handle_session_active;
	wl_signal_add(&session->events.active, &drm->session_active);

	if (!check_drm_features(drm)) {
		goto error_event;
	}

	if (!init_drm_resources(drm)) {
		goto error_event;
	}

	if (drm->parent && !init_mgpu_renderer(drm)) {
		goto error_mgpu_renderer;
	}

	drm->session_destroy.notify = handle_session_destroy;
	wl_signal_add(&session->events.destroy, &drm->session_destroy);

	return &drm->backend;

error_mgpu_renderer:
	finish_drm_renderer(&drm->mgpu_renderer);
	finish_drm_resources(drm);
error_event:
	wl_list_remove(&drm->session_active.link);
	wl_event_source_remove(drm->drm_event);
error_fd:
	wl_list_remove(&drm->dev_remove.link);
	wl_list_remove(&drm->dev_change.link);
	wl_list_remove(&drm->parent_destroy.link);
	wlr_session_close_file(drm->session, dev);
	free(drm->name);
	free(drm);
	return NULL;
}
