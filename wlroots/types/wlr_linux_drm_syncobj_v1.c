#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wlr/render/drm_syncobj.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/util/log.h>
#include <xf86drm.h>
#include "config.h"
#include "linux-drm-syncobj-v1-protocol.h"

#define LINUX_DRM_SYNCOBJ_V1_VERSION 1

struct wlr_linux_drm_syncobj_surface_v1 {
	struct wl_resource *resource;
	struct wlr_surface *surface;
	struct wlr_linux_drm_syncobj_surface_v1_state pending, current;

	struct wlr_addon addon;
	struct wlr_surface_synced synced;

	struct wl_listener client_commit;
};

struct wlr_linux_drm_syncobj_surface_v1_commit {
	struct wlr_surface *surface;
	struct wlr_drm_syncobj_timeline_waiter waiter;
	uint32_t cached_seq;

	struct wl_listener surface_destroy;
};

static const struct wp_linux_drm_syncobj_manager_v1_interface manager_impl;
static const struct wp_linux_drm_syncobj_timeline_v1_interface timeline_impl;
static const struct wp_linux_drm_syncobj_surface_v1_interface surface_impl;

static struct wlr_linux_drm_syncobj_manager_v1 *manager_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_linux_drm_syncobj_manager_v1_interface, &manager_impl));
	return wl_resource_get_user_data(resource);
}

static struct wlr_drm_syncobj_timeline *timeline_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_linux_drm_syncobj_timeline_v1_interface, &timeline_impl));
	return wl_resource_get_user_data(resource);
}

// Returns NULL if the syncobj surface is inert
static struct wlr_linux_drm_syncobj_surface_v1 *surface_from_resource(
		struct wl_resource *resource) {
	assert(wl_resource_instance_of(resource,
		&wp_linux_drm_syncobj_surface_v1_interface, &surface_impl));
	return wl_resource_get_user_data(resource);
}

static void timeline_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_drm_syncobj_timeline *timeline = timeline_from_resource(resource);
	wlr_drm_syncobj_timeline_unref(timeline);
}

static void timeline_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static const struct wp_linux_drm_syncobj_timeline_v1_interface timeline_impl = {
	.destroy = timeline_handle_destroy,
};

static void surface_destroy(struct wlr_linux_drm_syncobj_surface_v1 *surface) {
	if (surface == NULL) {
		return;
	}
	wl_list_remove(&surface->client_commit.link);
	wlr_addon_finish(&surface->addon);
	wlr_surface_synced_finish(&surface->synced);
	wl_resource_set_user_data(surface->resource, NULL);
	free(surface);
}

static void surface_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static void surface_handle_set_acquire_point(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *timeline_resource,
		uint32_t point_hi, uint32_t point_lo) {
	struct wlr_linux_drm_syncobj_surface_v1 *surface =
		surface_from_resource(resource);
	if (surface == NULL) {
		wl_resource_post_error(resource,
			WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE,
			"The surface has been destroyed");
		return;
	}

	struct wlr_drm_syncobj_timeline *timeline =
		timeline_from_resource(timeline_resource);
	uint64_t point = (uint64_t)point_hi << 32 | point_lo;

	wlr_drm_syncobj_timeline_unref(surface->pending.acquire_timeline);
	surface->pending.acquire_timeline = wlr_drm_syncobj_timeline_ref(timeline);
	surface->pending.acquire_point = point;
}

static void surface_handle_set_release_point(struct wl_client *client,
		struct wl_resource *resource, struct wl_resource *timeline_resource,
		uint32_t point_hi, uint32_t point_lo) {
	struct wlr_linux_drm_syncobj_surface_v1 *surface =
		surface_from_resource(resource);
	if (surface == NULL) {
		wl_resource_post_error(resource,
			WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_SURFACE,
			"The surface has been destroyed");
		return;
	}

	struct wlr_drm_syncobj_timeline *timeline =
		timeline_from_resource(timeline_resource);
	uint64_t point = (uint64_t)point_hi << 32 | point_lo;

	wlr_drm_syncobj_timeline_unref(surface->pending.release_timeline);
	surface->pending.release_timeline = wlr_drm_syncobj_timeline_ref(timeline);
	surface->pending.release_point = point;
}

static const struct wp_linux_drm_syncobj_surface_v1_interface surface_impl = {
	.destroy = surface_handle_destroy,
	.set_acquire_point = surface_handle_set_acquire_point,
	.set_release_point = surface_handle_set_release_point,
};

static void surface_handle_resource_destroy(struct wl_resource *resource) {
	struct wlr_linux_drm_syncobj_surface_v1 *surface =
		surface_from_resource(resource);
	surface_destroy(surface);
}

static void surface_addon_handle_surface_destroy(struct wlr_addon *addon) {
	struct wlr_linux_drm_syncobj_surface_v1 *surface =
		wl_container_of(addon, surface, addon);
	surface_destroy(surface);
}

static const struct wlr_addon_interface surface_addon_impl = {
	.name = "wp_linux_drm_syncobj_surface_v1",
	.destroy = surface_addon_handle_surface_destroy,
};

static void surface_synced_finish_state(void *_state) {
	struct wlr_linux_drm_syncobj_surface_v1_state *state = _state;
	wlr_drm_syncobj_timeline_unref(state->acquire_timeline);
	wlr_drm_syncobj_timeline_unref(state->release_timeline);
}

static void surface_synced_move_state(void *_dst, void *_src) {
	struct wlr_linux_drm_syncobj_surface_v1_state *dst = _dst, *src = _src;
	// TODO: immediately signal dst.release_timeline if necessary
	surface_synced_finish_state(dst);
	*dst = *src;
	*src = (struct wlr_linux_drm_syncobj_surface_v1_state){0};
}

static const struct wlr_surface_synced_impl surface_synced_impl = {
	.state_size = sizeof(struct wlr_linux_drm_syncobj_surface_v1_state),
	.finish_state = surface_synced_finish_state,
	.move_state = surface_synced_move_state,
};

static void manager_handle_destroy(struct wl_client *client,
		struct wl_resource *resource) {
	wl_resource_destroy(resource);
}

static struct wlr_linux_drm_syncobj_surface_v1 *surface_from_wlr_surface(
		struct wlr_surface *wlr_surface) {
	struct wlr_addon *addon =
		wlr_addon_find(&wlr_surface->addons, NULL, &surface_addon_impl);
	if (addon == NULL) {
		return NULL;
	}
	struct wlr_linux_drm_syncobj_surface_v1 *surface =
		wl_container_of(addon, surface, addon);
	return surface;
}

static void surface_commit_destroy(struct wlr_linux_drm_syncobj_surface_v1_commit *commit) {
	wlr_surface_unlock_cached(commit->surface, commit->cached_seq);
	wl_list_remove(&commit->surface_destroy.link);
	wlr_drm_syncobj_timeline_waiter_finish(&commit->waiter);
	free(commit);
}

static void surface_commit_handle_waiter_ready(struct wlr_drm_syncobj_timeline_waiter *waiter) {
	struct wlr_linux_drm_syncobj_surface_v1_commit *commit =
		wl_container_of(waiter, commit, waiter);
	surface_commit_destroy(commit);
}

static void surface_commit_handle_surface_destroy(struct wl_listener *listener,
		void *data) {
	struct wlr_linux_drm_syncobj_surface_v1_commit *commit =
		wl_container_of(listener, commit, surface_destroy);
	surface_commit_destroy(commit);
}

// Block the surface commit until the fence materializes
static bool lock_surface_commit(struct wlr_linux_drm_syncobj_surface_v1 *surface,
		struct wlr_drm_syncobj_timeline *timeline, uint64_t point) {
	uint32_t flags = DRM_SYNCOBJ_WAIT_FLAGS_WAIT_AVAILABLE;

	bool already_materialized = false;
	if (!wlr_drm_syncobj_timeline_check(timeline, point, flags, &already_materialized)) {
		return false;
	} else if (already_materialized) {
		return true;
	}

	struct wlr_linux_drm_syncobj_surface_v1_commit *commit = calloc(1, sizeof(*commit));
	if (commit == NULL) {
		return false;
	}

	struct wl_client *client = wl_resource_get_client(surface->resource);
	struct wl_display *display = wl_client_get_display(client);
	struct wl_event_loop *loop = wl_display_get_event_loop(display);
	if (!wlr_drm_syncobj_timeline_waiter_init(&commit->waiter, timeline, point,
			flags, loop, surface_commit_handle_waiter_ready)) {
		free(commit);
		return false;
	}

	commit->surface = surface->surface;
	commit->cached_seq = wlr_surface_lock_pending(surface->surface);

	commit->surface_destroy.notify = surface_commit_handle_surface_destroy;
	wl_signal_add(&surface->surface->events.destroy, &commit->surface_destroy);

	return true;
}

static void surface_handle_client_commit(struct wl_listener *listener,
		void *data) {
	struct wlr_linux_drm_syncobj_surface_v1 *surface =
		wl_container_of(listener, surface, client_commit);

	if (surface->pending.acquire_timeline != NULL &&
			surface->surface->pending.buffer == NULL) {
		wlr_surface_reject_pending(surface->surface, surface->resource,
			WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER,
			"Acquire point set but no buffer attached");
		return;
	}
	if (surface->pending.release_timeline != NULL &&
			surface->surface->pending.buffer == NULL) {
		wlr_surface_reject_pending(surface->surface, surface->resource,
			WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_BUFFER,
			"Release point set but no buffer attached");
		return;
	}

	if (surface->pending.acquire_timeline == NULL &&
			surface->surface->pending.buffer != NULL) {
		wlr_surface_reject_pending(surface->surface, surface->resource,
			WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_ACQUIRE_POINT,
			"Buffer attached but no acquire point set");
		return;
	}
	if (surface->pending.release_timeline == NULL &&
			surface->surface->pending.buffer != NULL) {
		wlr_surface_reject_pending(surface->surface, surface->resource,
			WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_NO_RELEASE_POINT,
			"Buffer attached but no release point set");
		return;
	}

	if (surface->pending.acquire_timeline != NULL &&
			surface->pending.release_timeline != NULL &&
			surface->pending.acquire_timeline == surface->pending.release_timeline &&
			surface->pending.acquire_point >= surface->pending.release_point) {
		wlr_surface_reject_pending(surface->surface, surface->resource,
			WP_LINUX_DRM_SYNCOBJ_SURFACE_V1_ERROR_CONFLICTING_POINTS,
			"Acquire and release points conflict");
		return;
	}

	if (surface->pending.acquire_timeline != NULL && !lock_surface_commit(
			surface, surface->pending.acquire_timeline, surface->pending.acquire_point)) {
		wl_resource_post_no_memory(surface->resource);
		return;
	}
}

static void manager_handle_get_surface(struct wl_client *client,
		struct wl_resource *resource, uint32_t id,
		struct wl_resource *surface_resource) {
	struct wlr_surface *wlr_surface = wlr_surface_from_resource(surface_resource);

	if (surface_from_wlr_surface(wlr_surface) != NULL) {
		wl_resource_post_error(resource,
			WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_SURFACE_EXISTS,
			"wp_linux_drm_syncobj_surface_v1 already created for this surface");
		return;
	}

	struct wlr_linux_drm_syncobj_surface_v1 *surface = calloc(1, sizeof(*surface));
	if (surface == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}

	if (!wlr_surface_synced_init(&surface->synced, wlr_surface,
			&surface_synced_impl, &surface->pending, &surface->current)) {
		goto error_surface;
	}

	uint32_t version = wl_resource_get_version(resource);
	surface->resource = wl_resource_create(client,
		&wp_linux_drm_syncobj_surface_v1_interface, version, id);
	if (surface->resource == NULL) {
		goto error_surface_synced;
	}
	wl_resource_set_implementation(surface->resource,
		&surface_impl, surface, surface_handle_resource_destroy);

	surface->surface = wlr_surface;

	surface->client_commit.notify = surface_handle_client_commit;
	wl_signal_add(&wlr_surface->events.client_commit, &surface->client_commit);

	wlr_addon_init(&surface->addon, &wlr_surface->addons, NULL, &surface_addon_impl);

	return;

error_surface_synced:
	wlr_surface_synced_finish(&surface->synced);
error_surface:
	free(surface);
	wl_resource_post_no_memory(resource);
}

static void manager_handle_import_timeline(struct wl_client *client,
		struct wl_resource *resource, uint32_t id, int drm_syncobj_fd) {
	struct wlr_linux_drm_syncobj_manager_v1 *manager =
		manager_from_resource(resource);

	struct wlr_drm_syncobj_timeline *timeline =
		wlr_drm_syncobj_timeline_import(manager->drm_fd, drm_syncobj_fd);
	close(drm_syncobj_fd);
	if (timeline == NULL) {
		wl_resource_post_error(resource,
			WP_LINUX_DRM_SYNCOBJ_MANAGER_V1_ERROR_INVALID_TIMELINE,
			"Failed to import drm_syncobj timeline");
		return;
	}

	uint32_t version = wl_resource_get_version(resource);
	struct wl_resource *timeline_resource = wl_resource_create(client,
		&wp_linux_drm_syncobj_timeline_v1_interface, version, id);
	if (timeline_resource == NULL) {
		wl_resource_post_no_memory(resource);
		return;
	}
	wl_resource_set_implementation(timeline_resource,
		&timeline_impl, timeline, timeline_handle_resource_destroy);
}

static const struct wp_linux_drm_syncobj_manager_v1_interface manager_impl = {
	.destroy = manager_handle_destroy,
	.get_surface = manager_handle_get_surface,
	.import_timeline = manager_handle_import_timeline,
};

static void manager_bind(struct wl_client *client, void *data,
		uint32_t version, uint32_t id) {
	struct wlr_linux_drm_syncobj_manager_v1 *manager = data;

	struct wl_resource *resource = wl_resource_create(client,
		&wp_linux_drm_syncobj_manager_v1_interface, version, id);
	if (resource == NULL) {
		wl_client_post_no_memory(client);
		return;
	}
	wl_resource_set_implementation(resource, &manager_impl, manager, NULL);
}

static void handle_display_destroy(struct wl_listener *listener, void *data) {
	struct wlr_linux_drm_syncobj_manager_v1 *manager =
		wl_container_of(listener, manager, display_destroy);
	wl_list_remove(&manager->display_destroy.link);
	wl_global_destroy(manager->global);
	close(manager->drm_fd);
	free(manager);
}

static bool check_syncobj_eventfd(int drm_fd) {
	if (!HAVE_EVENTFD) {
		return false;
	}

	uint64_t value = 0;
	if (drmGetCap(drm_fd, DRM_CAP_SYNCOBJ_TIMELINE, &value) != 0 || value == 0) {
		return false;
	}

	// Try waiting with invalid syncobj should fail with ENOENT
	struct drm_syncobj_eventfd syncobj_eventfd = {
		.handle = 0,
		.fd = -1,
	};
	return drmIoctl(drm_fd, DRM_IOCTL_SYNCOBJ_EVENTFD, &syncobj_eventfd) != 0 && errno == ENOENT;
}

struct wlr_linux_drm_syncobj_manager_v1 *wlr_linux_drm_syncobj_manager_v1_create(
		struct wl_display *display, uint32_t version, int drm_fd) {
	assert(version <= LINUX_DRM_SYNCOBJ_V1_VERSION);

	if (!check_syncobj_eventfd(drm_fd)) {
		wlr_log(WLR_INFO, "DRM syncobj eventfd unavailable, disabling linux-drm-syncobj-v1");
		return NULL;
	}

	struct wlr_linux_drm_syncobj_manager_v1 *manager = calloc(1, sizeof(*manager));
	if (manager == NULL) {
		return NULL;
	}

	manager->drm_fd = fcntl(drm_fd, F_DUPFD_CLOEXEC, 0);
	if (manager->drm_fd < 0) {
		goto error_manager;
	}

	manager->global = wl_global_create(display,
		&wp_linux_drm_syncobj_manager_v1_interface,
		version, manager, manager_bind);
	if (manager->global == NULL) {
		goto error_drm_fd;
	}

	manager->display_destroy.notify = handle_display_destroy;
	wl_display_add_destroy_listener(display, &manager->display_destroy);

	return manager;

error_drm_fd:
	close(manager->drm_fd);
error_manager:
	free(manager);
	return NULL;
}

struct wlr_linux_drm_syncobj_surface_v1_state *
wlr_linux_drm_syncobj_v1_get_surface_state(struct wlr_surface *wlr_surface) {
	struct wlr_linux_drm_syncobj_surface_v1 *surface =
		surface_from_wlr_surface(wlr_surface);
	if (surface == NULL) {
		return NULL;
	}
	return &surface->current;
}

struct release_signaller {
	struct wlr_drm_syncobj_timeline *timeline;
	uint64_t point;
	struct wl_listener buffer_release;
};

static void release_signaller_handle_buffer_release(struct wl_listener *listener, void *data) {
	struct release_signaller *signaller = wl_container_of(listener, signaller, buffer_release);

	if (drmSyncobjTimelineSignal(signaller->timeline->drm_fd, &signaller->timeline->handle,
			&signaller->point, 1) != 0) {
		wlr_log(WLR_ERROR, "drmSyncobjTimelineSignal() failed");
	}

	wlr_drm_syncobj_timeline_unref(signaller->timeline);
	wl_list_remove(&signaller->buffer_release.link);
	free(signaller);
}

bool wlr_linux_drm_syncobj_v1_state_signal_release_with_buffer(
		struct wlr_linux_drm_syncobj_surface_v1_state *state, struct wlr_buffer *buffer) {
	assert(buffer->n_locks > 0);
	if (state->release_timeline == NULL) {
		// This can happen if an existing surface with a buffer has a
		// syncobj_surface_v1_state created but no new buffer with release
		// timeline committed.
		return true;
	}

	struct release_signaller *signaller = calloc(1, sizeof(*signaller));
	if (signaller == NULL) {
		return false;
	}

	signaller->timeline = wlr_drm_syncobj_timeline_ref(state->release_timeline);
	signaller->point = state->release_point;

	signaller->buffer_release.notify = release_signaller_handle_buffer_release;
	wl_signal_add(&buffer->events.release, &signaller->buffer_release);

	return true;
}
