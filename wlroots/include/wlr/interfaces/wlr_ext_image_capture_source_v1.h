/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_INTERFACES_WLR_EXT_IMAGE_CAPTURE_SOURCE_V1_H
#define WLR_INTERFACES_WLR_EXT_IMAGE_CAPTURE_SOURCE_V1_H

#include <wayland-server-core.h>
#include <wlr/types/wlr_ext_image_capture_source_v1.h>

struct wlr_ext_image_copy_capture_frame_v1;
struct wlr_swapchain;
struct wlr_renderer;
struct wlr_seat;

struct wlr_ext_image_capture_source_v1_interface {
	// TODO: drop with_cursors flag
	void (*start)(struct wlr_ext_image_capture_source_v1 *source, bool with_cursors);
	void (*stop)(struct wlr_ext_image_capture_source_v1 *source);
	void (*schedule_frame)(struct wlr_ext_image_capture_source_v1 *source);
	void (*copy_frame)(struct wlr_ext_image_capture_source_v1 *source,
		struct wlr_ext_image_copy_capture_frame_v1 *dst_frame,
		struct wlr_ext_image_capture_source_v1_frame_event *frame_event);
	struct wlr_ext_image_capture_source_v1_cursor *(*get_pointer_cursor)(
		struct wlr_ext_image_capture_source_v1 *source, struct wlr_seat *seat);
};

void wlr_ext_image_capture_source_v1_init(struct wlr_ext_image_capture_source_v1 *source,
		const struct wlr_ext_image_capture_source_v1_interface *impl);
void wlr_ext_image_capture_source_v1_finish(struct wlr_ext_image_capture_source_v1 *source);
bool wlr_ext_image_capture_source_v1_create_resource(struct wlr_ext_image_capture_source_v1 *source,
	struct wl_client *client, uint32_t new_id);
bool wlr_ext_image_capture_source_v1_set_constraints_from_swapchain(
	struct wlr_ext_image_capture_source_v1 *source,
	struct wlr_swapchain *swapchain, struct wlr_renderer *renderer);

void wlr_ext_image_capture_source_v1_cursor_init(struct wlr_ext_image_capture_source_v1_cursor *source_cursor,
		const struct wlr_ext_image_capture_source_v1_interface *impl);
void wlr_ext_image_capture_source_v1_cursor_finish(struct wlr_ext_image_capture_source_v1_cursor *source_cursor);

#endif
