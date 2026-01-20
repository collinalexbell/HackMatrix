/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_OUTPUT_H
#define WLR_TYPES_WLR_OUTPUT_H

#include <pixman.h>
#include <stdbool.h>
#include <time.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <wlr/render/color.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/util/addon.h>
#include <wlr/util/box.h>

enum wlr_output_mode_aspect_ratio {
	WLR_OUTPUT_MODE_ASPECT_RATIO_NONE,
	WLR_OUTPUT_MODE_ASPECT_RATIO_4_3,
	WLR_OUTPUT_MODE_ASPECT_RATIO_16_9,
	WLR_OUTPUT_MODE_ASPECT_RATIO_64_27,
	WLR_OUTPUT_MODE_ASPECT_RATIO_256_135,
};

struct wlr_output_mode {
	int32_t width, height;
	int32_t refresh; // mHz
	bool preferred;
	enum wlr_output_mode_aspect_ratio picture_aspect_ratio;
	struct wl_list link;
};

struct wlr_output_cursor {
	struct wlr_output *output;
	double x, y;
	bool enabled;
	bool visible;
	uint32_t width, height;
	struct wlr_fbox src_box;
	enum wl_output_transform transform;
	int32_t hotspot_x, hotspot_y;
	struct wlr_texture *texture;
	bool own_texture;
	struct wlr_drm_syncobj_timeline *wait_timeline;
	uint64_t wait_point;
	struct wl_list link;

	struct {
		struct wl_listener renderer_destroy;
		struct wlr_color_transform *color_transform;
	} WLR_PRIVATE;
};

enum wlr_output_adaptive_sync_status {
	WLR_OUTPUT_ADAPTIVE_SYNC_DISABLED,
	WLR_OUTPUT_ADAPTIVE_SYNC_ENABLED,
};

enum wlr_output_state_field {
	WLR_OUTPUT_STATE_BUFFER = 1 << 0,
	WLR_OUTPUT_STATE_DAMAGE = 1 << 1,
	WLR_OUTPUT_STATE_MODE = 1 << 2,
	WLR_OUTPUT_STATE_ENABLED = 1 << 3,
	WLR_OUTPUT_STATE_SCALE = 1 << 4,
	WLR_OUTPUT_STATE_TRANSFORM = 1 << 5,
	WLR_OUTPUT_STATE_ADAPTIVE_SYNC_ENABLED = 1 << 6,
	WLR_OUTPUT_STATE_RENDER_FORMAT = 1 << 7,
	WLR_OUTPUT_STATE_SUBPIXEL = 1 << 8,
	WLR_OUTPUT_STATE_LAYERS = 1 << 9,
	WLR_OUTPUT_STATE_WAIT_TIMELINE = 1 << 10,
	WLR_OUTPUT_STATE_SIGNAL_TIMELINE = 1 << 11,
	WLR_OUTPUT_STATE_COLOR_TRANSFORM = 1 << 12,
	WLR_OUTPUT_STATE_IMAGE_DESCRIPTION = 1 << 13,
};

enum wlr_output_state_mode_type {
	WLR_OUTPUT_STATE_MODE_FIXED,
	WLR_OUTPUT_STATE_MODE_CUSTOM,
};

/**
 * Colorimetric image description.
 *
 * Carries information about the color encoding used for a struct wlr_buffer.
 *
 * Supported primaries are advertised in wlr_output.supported_primaries.
 * Supported transfer functions are advertised in
 * wlr_output.supported_transfer_functions.
 *
 * mastering_display_primaries, mastering_luminance, max_cll and max_fall are
 * optional. Luminances are given in cd/m².
 */
struct wlr_output_image_description {
	enum wlr_color_named_primaries primaries;
	enum wlr_color_transfer_function transfer_function;

	struct wlr_color_primaries mastering_display_primaries;
	struct {
		double min, max;
	} mastering_luminance;
	double max_cll; // max content light level
	double max_fall; // max frame-average light level
};

/**
 * Holds the double-buffered output state.
 */
struct wlr_output_state {
	uint32_t committed; // enum wlr_output_state_field
	// Set to true to allow output reconfiguration to occur which may result
	// in temporary output disruptions and content misrepresentations.
	bool allow_reconfiguration;
	pixman_region32_t damage; // output-buffer-local coordinates
	bool enabled;
	float scale;
	enum wl_output_transform transform;
	bool adaptive_sync_enabled;
	uint32_t render_format;
	enum wl_output_subpixel subpixel;

	struct wlr_buffer *buffer;
	// Source crop for the buffer.  If all zeros then no crop is applied.
	// As usual with source crop, this is in buffer coordinates.
	// Double-buffered by WLR_OUTPUT_STATE_BUFFER along with `buffer`.
	struct wlr_fbox buffer_src_box;
	// Destination rect to scale the buffer to (after source crop).  If width
	// and height are zero then the buffer is displayed at native size.  The
	// offset is relative to the origin of this output. Double-buffered by
	// WLR_OUTPUT_STATE_BUFFER along with `buffer`.
	struct wlr_box buffer_dst_box;

	/* Request a tearing page-flip. When enabled, this may cause the output to
	 * display a part of the previous buffer and a part of the current buffer at
	 * the same time. The backend may reject the commit if a tearing page-flip
	 * cannot be performed, in which case the caller should fall back to a
	 * regular page-flip at the next wlr_output.frame event. */
	bool tearing_page_flip;

	enum wlr_output_state_mode_type mode_type;
	struct wlr_output_mode *mode;
	struct {
		int32_t width, height;
		int32_t refresh; // mHz, may be zero
	} custom_mode;

	struct wlr_output_layer_state *layers;
	size_t layers_len;

	struct wlr_drm_syncobj_timeline *wait_timeline;
	uint64_t wait_point;
	struct wlr_drm_syncobj_timeline *signal_timeline;
	uint64_t signal_point;

	struct wlr_color_transform *color_transform;

	struct wlr_output_image_description *image_description;
};

struct wlr_output_impl;
struct wlr_render_pass;

/**
 * A compositor output region. This typically corresponds to a monitor that
 * displays part of the compositor space.
 *
 * The `frame` event will be emitted when it is a good time for the compositor
 * to submit a new frame.
 *
 * To render a new frame compositors should call wlr_output_begin_render_pass(),
 * perform rendering on that render pass, and finally call
 * wlr_output_commit_state().
 */
struct wlr_output {
	const struct wlr_output_impl *impl;
	struct wlr_backend *backend;
	struct wl_event_loop *event_loop;

	struct wl_global *global;
	struct wl_list resources;

	char *name;
	char *description; // may be NULL
	char *make, *model, *serial; // may be NULL
	int32_t phys_width, phys_height; // mm
	const struct wlr_color_primaries *default_primaries; // may be NULL

	// Note: some backends may have zero modes
	struct wl_list modes; // wlr_output_mode.link
	struct wlr_output_mode *current_mode;
	int32_t width, height;
	int32_t refresh; // mHz, may be zero

	uint32_t supported_primaries; // bitfield of enum wlr_color_named_primaries
	uint32_t supported_transfer_functions; // bitfield of enum wlr_color_transfer_function

	bool enabled;
	float scale;
	enum wl_output_subpixel subpixel;
	enum wl_output_transform transform;
	enum wlr_output_adaptive_sync_status adaptive_sync_status;
	uint32_t render_format;
	const struct wlr_output_image_description *image_description;

	// Indicates whether making changes to adaptive sync status is supported.
	// If false, changes to adaptive sync status is guaranteed to fail. If
	// true, changes may either succeed or fail.
	bool adaptive_sync_supported;

	bool needs_frame;
	// damage for cursors and fullscreen surface, in output-local coordinates
	bool frame_pending;

	// true for example with VR headsets
	bool non_desktop;

	// Commit sequence number. Incremented on each commit, may overflow.
	uint32_t commit_seq;

	struct {
		// Request to render a frame
		struct wl_signal frame;
		// Emitted when software cursors or backend-specific logic damage the
		// output
		struct wl_signal damage; // struct wlr_output_event_damage
		// Emitted when a new frame needs to be committed (because of
		// backend-specific logic)
		struct wl_signal needs_frame;
		// Emitted right before commit
		struct wl_signal precommit; // struct wlr_output_event_precommit
		// Emitted right after commit
		struct wl_signal commit; // struct wlr_output_event_commit
		// Emitted right after a commit has been presented to the user for
		// enabled outputs
		struct wl_signal present; // struct wlr_output_event_present
		// Emitted after a client bound the wl_output global
		struct wl_signal bind; // struct wlr_output_event_bind
		struct wl_signal description;
		struct wl_signal request_state; // struct wlr_output_event_request_state
		struct wl_signal destroy;
	} events;

	struct wl_event_source *idle_frame;
	struct wl_event_source *idle_done;

	int attach_render_locks; // number of locks forcing rendering

	struct wl_list cursors; // wlr_output_cursor.link
	struct wlr_output_cursor *hardware_cursor;
	struct wlr_swapchain *cursor_swapchain;
	struct wlr_buffer *cursor_front_buffer;
	int software_cursor_locks; // number of locks forcing software cursors

	struct wl_list layers; // wlr_output_layer.link

	struct wlr_allocator *allocator;
	struct wlr_renderer *renderer;
	struct wlr_swapchain *swapchain;

	struct wlr_addon_set addons;

	void *data;

	struct {
		struct wl_listener display_destroy;
		struct wlr_output_image_description image_description_value;
		struct wlr_color_transform *color_transform;
		struct wlr_color_primaries default_primaries_value;
	} WLR_PRIVATE;
};

struct wlr_output_event_damage {
	struct wlr_output *output;
	const pixman_region32_t *damage; // output-buffer-local coordinates
};

struct wlr_output_event_precommit {
	struct wlr_output *output;
	struct timespec when;
	const struct wlr_output_state *state;
};

struct wlr_output_event_commit {
	struct wlr_output *output;
	struct timespec when;
	const struct wlr_output_state *state;
};

enum wlr_output_present_flag {
	// The presentation was synchronized to the "vertical retrace" by the
	// display hardware such that tearing does not happen.
	WLR_OUTPUT_PRESENT_VSYNC = 0x1,
	// The display hardware provided measurements that the hardware driver
	// converted into a presentation timestamp.
	WLR_OUTPUT_PRESENT_HW_CLOCK = 0x2,
	// The display hardware signalled that it started using the new image
	// content.
	WLR_OUTPUT_PRESENT_HW_COMPLETION = 0x4,
	// The presentation of this update was done zero-copy.
	WLR_OUTPUT_PRESENT_ZERO_COPY = 0x8,
};

struct wlr_output_event_present {
	struct wlr_output *output;
	// Frame submission for which this presentation event is for (see
	// wlr_output.commit_seq).
	uint32_t commit_seq;
	// Whether the frame was presented at all.
	bool presented;
	// Time when the content update turned into light the first time.
	struct timespec when;
	// Vertical retrace counter. Zero if unavailable.
	unsigned seq;
	// Prediction of how many nanoseconds after `when` the very next output
	// refresh may occur. Zero if unknown.
	int refresh; // nsec
	uint32_t flags; // enum wlr_output_present_flag
};

struct wlr_output_event_bind {
	struct wlr_output *output;
	struct wl_resource *resource;
};

struct wlr_output_event_request_state {
	struct wlr_output *output;
	const struct wlr_output_state *state;
};

void wlr_output_create_global(struct wlr_output *output, struct wl_display *display);
void wlr_output_destroy_global(struct wlr_output *output);
/**
 * Initialize the output's rendering subsystem with the provided allocator and
 * renderer. After initialization, this function may invoked again to reinitialize
 * the allocator and renderer to different values.
 *
 * Call this function prior to any call to wlr_output_begin_render_pass(),
 * wlr_output_commit_state() or wlr_output_cursor_create().
 *
 * The buffer capabilities of the provided must match the capabilities of the
 * output's backend. Returns false otherwise.
 */
bool wlr_output_init_render(struct wlr_output *output,
	struct wlr_allocator *allocator, struct wlr_renderer *renderer);
/**
 * Returns the preferred mode for this output. If the output doesn't support
 * modes, returns NULL.
 */
struct wlr_output_mode *wlr_output_preferred_mode(struct wlr_output *output);
/**
 * Set the output name.
 *
 * Output names are subject to the following rules:
 *
 * - Each output name must be unique.
 * - The name cannot change after the output has been advertised to clients.
 *
 * For more details, see the protocol documentation for wl_output.name.
 */
void wlr_output_set_name(struct wlr_output *output, const char *name);
void wlr_output_set_description(struct wlr_output *output, const char *desc);
/**
 * Schedule a done event.
 *
 * This is intended to be used by wl_output add-on interfaces.
 */
void wlr_output_schedule_done(struct wlr_output *output);
void wlr_output_destroy(struct wlr_output *output);
/**
 * Computes the transformed output resolution.
 */
void wlr_output_transformed_resolution(struct wlr_output *output,
	int *width, int *height);
/**
 * Computes the transformed and scaled output resolution.
 */
void wlr_output_effective_resolution(struct wlr_output *output,
	int *width, int *height);
/**
 * Test whether this output state would be accepted by the backend. If this
 * function returns true, wlr_output_commit_state() will only fail due to a
 * runtime error. This function does not change the current state of the
 * output.
 */
bool wlr_output_test_state(struct wlr_output *output,
	const struct wlr_output_state *state);
/**
 * Attempts to apply the state to this output. This function may fail for any
 * reason and return false. If failed, none of the state would have been applied,
 * this function is atomic. If the commit succeeded, true is returned.
 *
 * Note: wlr_output_state_finish() would typically be called after the state
 * has been committed.
 */
bool wlr_output_commit_state(struct wlr_output *output,
	const struct wlr_output_state *state);
/**
 * Manually schedules a `frame` event. If a `frame` event is already pending,
 * it is a no-op.
 */
void wlr_output_schedule_frame(struct wlr_output *output);
/**
 * Returns the maximum length of each gamma ramp, or 0 if unsupported.
 */
size_t wlr_output_get_gamma_size(struct wlr_output *output);
/**
 * Returns the wlr_output matching the provided wl_output resource. If the
 * resource isn't a wl_output, it aborts. If the resource is inert (because the
 * wlr_output has been destroyed), NULL is returned.
 */
struct wlr_output *wlr_output_from_resource(struct wl_resource *resource);
/**
 * Locks the output to only use rendering instead of direct scan-out. This is
 * useful if direct scan-out needs to be temporarily disabled (e.g. during
 * screen capture). There must be as many unlocks as there have been locks to
 * restore the original state. There should never be an unlock before a lock.
 */
void wlr_output_lock_attach_render(struct wlr_output *output, bool lock);
/**
 * Locks the output to only use software cursors instead of hardware cursors.
 * This is useful if hardware cursors need to be temporarily disabled (e.g.
 * during screen capture). There must be as many unlocks as there have been
 * locks to restore the original state. There should never be an unlock before
 * a lock.
 */
void wlr_output_lock_software_cursors(struct wlr_output *output, bool lock);
/**
 * Render software cursors. The damage is in buffer-local coordinate space.
 *
 * This is a utility function that can be called when compositors render.
 */
void wlr_output_add_software_cursors_to_render_pass(struct wlr_output *output,
	struct wlr_render_pass *render_pass, const pixman_region32_t *damage);
/**
 * Get the set of DRM formats suitable for the primary buffer, assuming a
 * buffer with the specified capabilities.
 *
 * NULL is returned if the backend doesn't have any format constraint, ie. all
 * formats are supported. An empty set is returned if the backend doesn't
 * support any format.
 */
const struct wlr_drm_format_set *wlr_output_get_primary_formats(
	struct wlr_output *output, uint32_t buffer_caps);
/**
 * Check whether direct scan-out is allowed on the output.
 *
 * Direct scan-out is typically disallowed when there are software cursors or
 * during screen capture.
 */
bool wlr_output_is_direct_scanout_allowed(struct wlr_output *output);


struct wlr_output_cursor *wlr_output_cursor_create(struct wlr_output *output);
bool wlr_output_cursor_set_buffer(struct wlr_output_cursor *cursor,
	struct wlr_buffer *buffer, int32_t hotspot_x, int32_t hotspot_y);
bool wlr_output_cursor_move(struct wlr_output_cursor *cursor,
	double x, double y);
void wlr_output_cursor_destroy(struct wlr_output_cursor *cursor);

/**
 * Initialize an output state.
 */
void wlr_output_state_init(struct wlr_output_state *state);
/**
 * Releases all resources associated with an output state.
 */
void wlr_output_state_finish(struct wlr_output_state *state);
/**
 * Enables or disables an output. A disabled output is turned off and doesn't
 * emit `frame` events.
 *
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_enabled(struct wlr_output_state *state,
	bool enabled);
/**
 * Sets the output mode of an output. An output mode will specify the resolution
 * and refresh rate, among other things.
 *
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_mode(struct wlr_output_state *state,
	struct wlr_output_mode *mode);
/**
 * Sets a custom output mode for an output. See wlr_output_state_set_mode()
 * for details.
 *
 * When the output advertises fixed modes, custom modes are not guaranteed to
 * work correctly, they may result in visual artifacts. If a suitable fixed mode
 * is available, compositors should prefer it and use wlr_output_state_set_mode()
 * instead of custom modes.
 *
 * Setting `refresh` to zero lets the backend pick a preferred value. The
 * output needs to be enabled.
 *
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_custom_mode(struct wlr_output_state *state,
	int32_t width, int32_t height, int32_t refresh);
/**
 * Sets the scale of an output. The scale is used to increase the size of UI
 * elements to aid users who use high DPI outputs.
 *
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_scale(struct wlr_output_state *state, float scale);
/**
 * Sets the transform of an output. The transform is used to rotate or flip
 * the contents of the screen.
 *
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_transform(struct wlr_output_state *state,
	enum wl_output_transform transform);
/**
 * Enables or disable adaptive sync for an output (ie. variable refresh rate).
 * Compositors can inspect `wlr_output.adaptive_sync_status` to query the
 * effective status. Backends that don't support adaptive sync will reject the
 * output commit.
 *
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_adaptive_sync_enabled(struct wlr_output_state *state,
	bool enabled);
/**
 * Sets the render format for an output.
 *
 * The default value is DRM_FORMAT_XRGB8888.
 *
 * While high bit depth render formats are necessary for a monitor to receive
 * useful high bit data, they do not guarantee it; a DRM_FORMAT_XBGR2101010
 * buffer will only lead to sending 10-bpc image data to the monitor if
 * hardware and software permit this.
 *
 * This only affects the format of the output buffer used when rendering using
 * the output's own swapchain as with wlr_output_begin_render_pass(). It has no
 * impact on the cursor buffer format, or on the formats supported for direct
 * scan-out (see also wlr_output_state_set_buffer()).
 *
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_render_format(struct wlr_output_state *state,
	uint32_t format);
/**
 * Sets the subpixel layout hint for an output. Note that this is only a hint
 * and may be ignored. The default value depends on the backend.
 *
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_subpixel(struct wlr_output_state *state,
	enum wl_output_subpixel subpixel);
/**
 * Sets the buffer for an output. The buffer contains the contents of the
 * screen. If the compositor wishes to present a new frame, they must commit
 * with a buffer.
 *
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_buffer(struct wlr_output_state *state,
	struct wlr_buffer *buffer);
/**
 * Sets the damage region for an output. This is used as a hint to the backend
 * and can be used to reduce power consumption or increase performance on some
 * devices.
 *
 * This should be called in along with wlr_output_state_set_buffer().
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_damage(struct wlr_output_state *state,
	const pixman_region32_t *damage);
/**
 * Set the output layers for an output. Output layers are used to try and take
 * advantage of backend features that may reduce the amount of things that
 * need to be composited.
 *
 * The array must be kept valid by the caller until wlr_output_state_finish()
 * and all copies of the state have been finished as well.
 * This state will be applied once wlr_output_commit_state() is called.
 */
void wlr_output_state_set_layers(struct wlr_output_state *state,
	struct wlr_output_layer_state *layers, size_t layers_len);
/**
 * Set a timeline point to wait on before displaying the next frame.
 *
 * Committing a wait timeline point without a buffer is invalid.
 *
 * There is only a single wait timeline point, waiting for multiple timeline
 * points is unsupported.
 *
 * Support for this feature is advertised by the timeline field in
 * struct wlr_output.
 */
void wlr_output_state_set_wait_timeline(struct wlr_output_state *state,
	struct wlr_drm_syncobj_timeline *timeline, uint64_t src_point);
/**
 * Set a timeline point to be signalled when the frame is no longer being used
 * by the backend.
 *
 * Committing a signal timeline point without a buffer is invalid.
 *
 * There is only a single signal timeline point, signalling multiple timeline
 * points is unsupported.
 *
 * Support for this feature is advertised by the timeline field in
 * struct wlr_output.
 */
void wlr_output_state_set_signal_timeline(struct wlr_output_state *state,
	struct wlr_drm_syncobj_timeline *timeline, uint64_t dst_point);
/**
 * Set the color transform for an output.
 *
 * The color transform is applied after blending output layers.
 */
void wlr_output_state_set_color_transform(struct wlr_output_state *state,
	struct wlr_color_transform *tr);

/**
 * Set the colorimetry image description.
 */
bool wlr_output_state_set_image_description(struct wlr_output_state *state,
	const struct wlr_output_image_description *image_desc);

/**
 * Copies the output state from src to dst. It is safe to then
 * wlr_output_state_finish() src and have dst still be valid.
 *
 * Note: The lifetime of the output layers inside the state are not managed. It
 * is the responsibility of the constructor of the output layers to make sure
 * they remain valid for the output state and all copies made.
 */
bool wlr_output_state_copy(struct wlr_output_state *dst,
	const struct wlr_output_state *src);


/**
 * Re-configure the swapchain as required for the output's primary buffer.
 *
 * If a NULL swapchain is passed in, a new swapchain is allocated. If the
 * swapchain is already suitable for the output's primary buffer, this function
 * is a no-op.
 *
 * The state describes the output changes the swapchain's buffers will be
 * committed with. A NULL state indicates no change.
 */
bool wlr_output_configure_primary_swapchain(struct wlr_output *output,
	const struct wlr_output_state *state, struct wlr_swapchain **swapchain);
/**
 * Begin a render pass on this output.
 *
 * Compositors can call this function to begin rendering. After the render pass
 * has been submitted, they should call wlr_output_commit_state() to submit the
 * new frame.
 *
 * On error, NULL is returned. Creating a render pass on a disabled output is
 * an error.
 *
 * The state describes the output changes the rendered frame will be
 * committed with. A NULL state indicates no change.
 *
 * If non-NULL, buffer_age is set to the drawing buffer age in number of
 * frames or -1 if unknown. This is useful for damage tracking.
 */
struct wlr_render_pass *wlr_output_begin_render_pass(struct wlr_output *output,
	struct wlr_output_state *state, struct wlr_buffer_pass_options *render_options);

#endif
