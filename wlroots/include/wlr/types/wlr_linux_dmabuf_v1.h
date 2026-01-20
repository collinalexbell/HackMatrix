/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_LINUX_DMABUF_V1_H
#define WLR_TYPES_WLR_LINUX_DMABUF_V1_H

#include <stdint.h>
#include <sys/stat.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/render/dmabuf.h>
#include <wlr/render/drm_format_set.h>

struct wlr_surface;

struct wlr_dmabuf_v1_buffer {
	struct wlr_buffer base;

	struct wl_resource *resource; // can be NULL if the client destroyed it
	struct wlr_dmabuf_attributes attributes;

	struct {
		struct wl_listener release;
	} WLR_PRIVATE;
};

/**
 * Returns the struct wlr_dmabuf_buffer if the given resource was created
 * via the linux-dmabuf buffer protocol or NULL otherwise.
 */
struct wlr_dmabuf_v1_buffer *wlr_dmabuf_v1_buffer_try_from_buffer_resource(
	struct wl_resource *buffer_resource);

struct wlr_linux_dmabuf_feedback_v1 {
	dev_t main_device;
	struct wl_array tranches; // struct wlr_linux_dmabuf_feedback_v1_tranche
};

struct wlr_linux_dmabuf_feedback_v1_tranche {
	dev_t target_device;
	uint32_t flags; // bitfield of enum zwp_linux_dmabuf_feedback_v1_tranche_flags
	struct wlr_drm_format_set formats;
};

/* the protocol interface */
struct wlr_linux_dmabuf_v1 {
	struct wl_global *global;

	struct {
		struct wl_signal destroy;
	} events;

	struct {
		struct wlr_linux_dmabuf_feedback_v1_compiled *default_feedback;
		struct wlr_drm_format_set default_formats; // for legacy clients
		struct wl_list surfaces; // wlr_linux_dmabuf_v1_surface.link

		int main_device_fd; // to sanity check FDs sent by clients, -1 if unavailable

		struct wl_listener display_destroy;

		bool (*check_dmabuf_callback)(struct wlr_dmabuf_attributes *attribs, void *data);
		void *check_dmabuf_callback_data;
	} WLR_PRIVATE;
};

/**
 * Create the linux-dmabuf-v1 global.
 *
 * Compositors using struct wlr_renderer should use
 * wlr_linux_dmabuf_v1_create_with_renderer() instead.
 */
struct wlr_linux_dmabuf_v1 *wlr_linux_dmabuf_v1_create(struct wl_display *display,
	uint32_t version, const struct wlr_linux_dmabuf_feedback_v1 *default_feedback);

/**
 * Create the linux-dmabuf-v1 global.
 *
 * The default DMA-BUF feedback is initialized from the struct wlr_renderer.
 */
struct wlr_linux_dmabuf_v1 *wlr_linux_dmabuf_v1_create_with_renderer(struct wl_display *display,
	uint32_t version, struct wlr_renderer *renderer);

/**
 * Set the dmabuf import check callback
 *
 * This can be used by consumers who want to implement specific dmabuf checks. Useful for
 * users such as gamescope who do not use the rendering logic of wlroots.
 */
void wlr_linux_dmabuf_v1_set_check_dmabuf_callback(struct wlr_linux_dmabuf_v1 *linux_dmabuf,
	bool (*callback)(struct wlr_dmabuf_attributes *attribs, void *data), void *data);

/**
 * Set a surface's DMA-BUF feedback.
 *
 * Passing a NULL feedback resets it to the default feedback.
 */
bool wlr_linux_dmabuf_v1_set_surface_feedback(
	struct wlr_linux_dmabuf_v1 *linux_dmabuf, struct wlr_surface *surface,
	const struct wlr_linux_dmabuf_feedback_v1 *feedback);

/**
 * Append a tranche at the end of the DMA-BUF feedback list.
 *
 * Tranches must be added with decreasing priority.
 */
struct wlr_linux_dmabuf_feedback_v1_tranche *wlr_linux_dmabuf_feedback_add_tranche(
	struct wlr_linux_dmabuf_feedback_v1 *feedback);

/**
 * Release resources allocated by a DMA-BUF feedback object.
 */
void wlr_linux_dmabuf_feedback_v1_finish(struct wlr_linux_dmabuf_feedback_v1 *feedback);

struct wlr_linux_dmabuf_feedback_v1_init_options {
	// Main renderer used by the compositor
	struct wlr_renderer *main_renderer;
	// Output on which direct scan-out is possible on the primary plane, or NULL
	struct wlr_output *scanout_primary_output;
	// Output layer feedback event, or NULL
	const struct wlr_output_layer_feedback_event *output_layer_feedback_event;
};

/**
 * Initialize a DMA-BUF feedback object with the provided options.
 *
 * The caller is responsible for calling wlr_linux_dmabuf_feedback_v1_finish() after use.
 */
bool wlr_linux_dmabuf_feedback_v1_init_with_options(struct wlr_linux_dmabuf_feedback_v1 *feedback,
	const struct wlr_linux_dmabuf_feedback_v1_init_options *options);

#endif
