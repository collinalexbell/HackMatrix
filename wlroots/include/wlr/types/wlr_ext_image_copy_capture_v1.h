/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_EXT_IMAGE_COPY_CAPTURE_V1_H
#define WLR_TYPES_WLR_EXT_IMAGE_COPY_CAPTURE_V1_H

#include <pixman.h>
#include <wayland-server-protocol.h>
#include <wayland-protocols/ext-image-copy-capture-v1-enum.h>
#include <time.h>

struct wlr_renderer;

struct wlr_ext_image_copy_capture_manager_v1 {
	struct wl_global *global;

	struct {
		struct wl_listener display_destroy;
	} WLR_PRIVATE;
};

struct wlr_ext_image_copy_capture_frame_v1 {
	struct wl_resource *resource;
	bool capturing;
	struct wlr_buffer *buffer;
	pixman_region32_t buffer_damage;

	struct {
		struct wl_signal destroy;
	} events;

	struct {
		struct wlr_ext_image_copy_capture_session_v1 *session;
	} WLR_PRIVATE;
};

struct wlr_ext_image_copy_capture_manager_v1 *wlr_ext_image_copy_capture_manager_v1_create(
	struct wl_display *display, uint32_t version);

/**
 * Notify the client that the frame is ready.
 *
 * This function destroys the frame.
 */
void wlr_ext_image_copy_capture_frame_v1_ready(struct wlr_ext_image_copy_capture_frame_v1 *frame,
	enum wl_output_transform transform, const struct timespec *presentation_time);
/**
 * Notify the client that the frame has failed.
 *
 * This function destroys the frame.
 */
void wlr_ext_image_copy_capture_frame_v1_fail(struct wlr_ext_image_copy_capture_frame_v1 *frame,
	enum ext_image_copy_capture_frame_v1_failure_reason reason);
/**
 * Copy a struct wlr_buffer into the client-provided buffer for the frame.
 */
bool wlr_ext_image_copy_capture_frame_v1_copy_buffer(struct wlr_ext_image_copy_capture_frame_v1 *frame,
	struct wlr_buffer *src, struct wlr_renderer *renderer);

#endif
