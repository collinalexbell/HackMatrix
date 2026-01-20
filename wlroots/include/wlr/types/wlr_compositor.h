/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_COMPOSITOR_H
#define WLR_TYPES_WLR_COMPOSITOR_H

#include <pixman.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <wayland-server-core.h>
#include <wlr/util/addon.h>
#include <wlr/util/box.h>

struct wlr_surface;

enum wlr_surface_state_field {
	WLR_SURFACE_STATE_BUFFER = 1 << 0,
	WLR_SURFACE_STATE_SURFACE_DAMAGE = 1 << 1,
	WLR_SURFACE_STATE_BUFFER_DAMAGE = 1 << 2,
	WLR_SURFACE_STATE_OPAQUE_REGION = 1 << 3,
	WLR_SURFACE_STATE_INPUT_REGION = 1 << 4,
	WLR_SURFACE_STATE_TRANSFORM = 1 << 5,
	WLR_SURFACE_STATE_SCALE = 1 << 6,
	WLR_SURFACE_STATE_FRAME_CALLBACK_LIST = 1 << 7,
	WLR_SURFACE_STATE_VIEWPORT = 1 << 8,
	WLR_SURFACE_STATE_OFFSET = 1 << 9,
};

struct wlr_surface_state {
	uint32_t committed; // enum wlr_surface_state_field
	// Sequence number of the surface state. Incremented on each commit, may
	// overflow.
	uint32_t seq;

	struct wlr_buffer *buffer;
	int32_t dx, dy; // relative to previous position
	pixman_region32_t surface_damage, buffer_damage; // clipped to bounds
	pixman_region32_t opaque, input;
	enum wl_output_transform transform;
	int32_t scale;
	struct wl_list frame_callback_list; // wl_resource

	int width, height; // in surface-local coordinates
	int buffer_width, buffer_height;

	struct wl_list subsurfaces_below;
	struct wl_list subsurfaces_above;

	/**
	 * The viewport is applied after the surface transform and scale.
	 *
	 * If has_src is true, the surface content is cropped to the provided
	 * rectangle. If has_dst is true, the surface content is scaled to the
	 * provided rectangle.
	 */
	struct {
		bool has_src, has_dst;
		// In coordinates after scale/transform are applied, but before the
		// destination rectangle is applied
		struct wlr_fbox src;
		int dst_width, dst_height; // in surface-local coordinates
	} viewport;

	// Number of locks that prevent this surface state from being committed.
	size_t cached_state_locks;
	struct wl_list cached_state_link; // wlr_surface.cached

	// Sync'ed object states, one per struct wlr_surface_synced
	struct wl_array synced; // void *
};

struct wlr_surface_role {
	const char *name;
	/**
	 * If true, the role isn't represented by any object.
	 * For example, this applies to cursor surfaces.
	 */
	bool no_object;
	/**
	 * Called when the client sends the wl_surface.commit request. May be NULL.
	 * Typically used to check that the pending state is valid, and send
	 * protocol errors if not.
	 *
	 * If the role is represented by an object, this is only called if
	 * such object exists.
	 */
	void (*client_commit)(struct wlr_surface *surface);
	/**
	 * Called when a new surface state is committed. May be NULL.
	 *
	 * If the role is represented by an object, this is only called if
	 * such object exists.
	 */
	void (*commit)(struct wlr_surface *surface);
	/**
	 * Called when the surface is mapped. May be NULL.
	 *
	 * If the role is represented by an object, this is only called if
	 * such object exists.
	 */
	void (*map)(struct wlr_surface *surface);
	/**
	 * Called when the surface is unmapped. May be NULL.
	 *
	 * If the role is represented by an object, this is only called if
	 * such object exists.
	 */
	void (*unmap)(struct wlr_surface *surface);
	/**
	 * Called when the object representing the role is destroyed. May be NULL.
	 */
	void (*destroy)(struct wlr_surface *surface);
};

struct wlr_surface_output {
	struct wlr_surface *surface;
	struct wlr_output *output;

	struct wl_list link; // wlr_surface.current_outputs

	struct {
		struct wl_listener bind;
		struct wl_listener destroy;
	} WLR_PRIVATE;
};

struct wlr_surface {
	struct wl_resource *resource;
	struct wlr_compositor *compositor;
	/**
	 * The surface's buffer, if any. A surface has an attached buffer when it
	 * commits with a non-null buffer in its pending state. A surface will not
	 * have a buffer if it has never committed one, has committed a null buffer,
	 * or something went wrong with uploading the buffer.
	 */
	struct wlr_client_buffer *buffer;
	/**
	 * The last commit's buffer damage, in buffer-local coordinates. This
	 * contains both the damage accumulated by the client via
	 * `wlr_surface_state.surface_damage` and `wlr_surface_state.buffer_damage`.
	 * If the buffer has been resized, the whole buffer is damaged.
	 *
	 * This region needs to be scaled and transformed into output coordinates,
	 * just like the buffer's texture. In addition, if the buffer has shrunk the
	 * old size needs to be damaged and if the buffer has moved the old and new
	 * positions need to be damaged.
	 */
	pixman_region32_t buffer_damage;
	/**
	 * The current opaque region, in surface-local coordinates. It is clipped to
	 * the surface bounds. If the surface's buffer is using a fully opaque
	 * format, this is set to the whole surface.
	 */
	pixman_region32_t opaque_region;
	/**
	 * The current input region, in surface-local coordinates. It is clipped to
	 * the surface bounds.
	 *
	 * If the protocol states that the input region is ignored, this is empty.
	 */
	pixman_region32_t input_region;
	/**
	 * `current` contains the current, committed surface state. `pending`
	 * accumulates state changes from the client between commits and shouldn't
	 * be accessed by the compositor directly.
	 */
	struct wlr_surface_state current, pending;

	struct wl_list cached; // wlr_surface_state.cached_link

	/**
	 * Whether the surface is ready to be displayed.
	 */
	bool mapped;

	/**
	 * The lifetime-bound role of the surface. NULL if the role was never set.
	 */
	const struct wlr_surface_role *role;

	/**
	 * The role object representing the role. NULL if the role isn't
	 * represented by any object or the object was destroyed.
	 */
	struct wl_resource *role_resource;

	struct {
		/**
		 * Signals that the client has sent a wl_surface.commit request.
		 *
		 * The state to be committed can be accessed in wlr_surface.pending.
		 *
		 * The commit may not be applied immediately, in which case it's marked
		 * as "cached" and put into a queue. See wlr_surface_lock_pending().
		 */
		struct wl_signal client_commit;
		/**
		 * Signals that a commit has been applied.
		 *
		 * The new state can be accessed in wlr_surface.current.
		 */
		struct wl_signal commit;

		/**
		 * Signals that the surface has a non-null buffer committed and is
		 * ready to be displayed.
		 */
		struct wl_signal map;
		/**
		 * Signals that the surface shouldn't be displayed anymore. This can
		 * happen when a null buffer is committed, the associated role object
		 * is destroyed, or when the role-specific conditions for the surface
		 * to be mapped no longer apply.
		 */
		struct wl_signal unmap;

		/**
		 * Signals that a new child sub-surface has been added.
		 *
		 * Note: unlike other new_* signals, new_subsurface is emitted when
		 * the subsurface is added to the parent surface's current state,
		 * not when the object is created.
		 */
		struct wl_signal new_subsurface; // struct wlr_subsurface

		/**
		 * Signals that the surface is being destroyed.
		 */
		struct wl_signal destroy;
	} events;

	struct wl_list current_outputs; // wlr_surface_output.link

	struct wlr_addon_set addons;
	void *data;

	struct {
		struct wl_listener role_resource_destroy;

		struct {
			int32_t scale;
			enum wl_output_transform transform;
			int width, height;
			int buffer_width, buffer_height;
		} previous;

		bool unmap_commit;

		bool opaque;

		bool handling_commit;
		bool pending_rejected;

		int32_t preferred_buffer_scale;
		bool preferred_buffer_transform_sent;
		enum wl_output_transform preferred_buffer_transform;

		struct wl_list synced; // wlr_surface_synced.link
		size_t synced_len;

		struct wl_resource *pending_buffer_resource;
		struct wl_listener pending_buffer_resource_destroy;
	} WLR_PRIVATE;
};

struct wlr_renderer;

struct wlr_compositor {
	struct wl_global *global;
	struct wlr_renderer *renderer; // may be NULL

	struct {
		struct wl_signal new_surface;
		struct wl_signal destroy;
	} events;

	struct {
		struct wl_listener display_destroy;
		struct wl_listener renderer_destroy;
	} WLR_PRIVATE;
};

typedef void (*wlr_surface_iterator_func_t)(struct wlr_surface *surface,
	int sx, int sy, void *data);

/**
 * Set the lifetime role for this surface.
 *
 * If the surface already has a different role and/or has a role object set,
 * the function fails and sends an error to the client.
 *
 * Returns true on success, false otherwise.
 */
bool wlr_surface_set_role(struct wlr_surface *surface, const struct wlr_surface_role *role,
	struct wl_resource *error_resource, uint32_t error_code);

/**
 * Set the role object for this surface. The surface must have a role and
 * no already set role object.
 *
 * When the resource is destroyed, the surface is unmapped,
 * wlr_surface_role.destroy is called and the role object is unset.
 */
void wlr_surface_set_role_object(struct wlr_surface *surface, struct wl_resource *role_resource);

/**
 * Map the surface. If the surface is already mapped, this is no-op.
 *
 * This function must only be used by surface role implementations.
 */
void wlr_surface_map(struct wlr_surface *surface);

/**
 * Unmap the surface. If the surface is already unmapped, this is no-op.
 *
 * This function must only be used by surface role implementations.
 */
void wlr_surface_unmap(struct wlr_surface *surface);

/**
 * Mark the pending state of a surface as rejected due to a protocol violation,
 * preventing it from being cached or committed.
 *
 * This function must only be used while processing a commit request.
 */
void wlr_surface_reject_pending(struct wlr_surface *surface, struct wl_resource *resource,
	uint32_t code, const char *msg, ...);

/**
 * Whether or not this surface currently has an attached buffer. A surface has
 * an attached buffer when it commits with a non-null buffer in its pending
 * state. A surface will not have a buffer if it has never committed one or has
 * committed a null buffer.
 */
bool wlr_surface_has_buffer(struct wlr_surface *surface);

/**
 * Check whether this surface state has an attached buffer.
 *
 * A surface has an attached buffer when the client commits with a non-null
 * buffer. A surface will not have a buffer if the client never committed one,
 * or committed a null buffer.
 *
 * Note that wlr_surface_state.buffer may be NULL even if this function returns
 * true: the buffer field is reset after commit, to allow the buffer to be
 * released to the client. Additionally, the buffer import or upload may fail.
 */
bool wlr_surface_state_has_buffer(const struct wlr_surface_state *state);

/**
 * Get the texture of the buffer currently attached to this surface. Returns
 * NULL if no buffer is currently attached or if something went wrong with
 * uploading the buffer.
 */
struct wlr_texture *wlr_surface_get_texture(struct wlr_surface *surface);

/**
 * Get the root of the subsurface tree for this surface.
 * May return the same surface passed if that surface is the root.
 * Never returns NULL.
 */
struct wlr_surface *wlr_surface_get_root_surface(struct wlr_surface *surface);

/**
 * Check if the surface accepts input events at the given surface-local
 * coordinates. Does not check the surface's subsurfaces.
 */
bool wlr_surface_point_accepts_input(struct wlr_surface *surface,
		double sx, double sy);

/**
 * Find a surface in this surface's tree that accepts input events and has all
 * parents mapped (except this surface, which can be unmapped) at the given
 * surface-local coordinates. Returns the surface and coordinates in the leaf
 * surface coordinate system or NULL if no surface is found at that location.
 */
struct wlr_surface *wlr_surface_surface_at(struct wlr_surface *surface,
		double sx, double sy, double *sub_x, double *sub_y);

/**
 * Notify the client that the surface has entered an output.
 *
 * This is a no-op if the surface has already entered the output.
 */
void wlr_surface_send_enter(struct wlr_surface *surface,
		struct wlr_output *output);

/**
 * Notify the client that the surface has left an output.
 *
 * This is a no-op if the surface has already left the output.
 */
void wlr_surface_send_leave(struct wlr_surface *surface,
		struct wlr_output *output);

/**
 * Complete the queued frame callbacks for this surface.
 *
 * This will send an event to the client indicating that now is a good time to
 * draw its next frame.
 */
void wlr_surface_send_frame_done(struct wlr_surface *surface,
		const struct timespec *when);

/**
 * Get the bounding box that contains the surface and all subsurfaces in
 * surface coordinates.
 * X and y may be negative, if there are subsurfaces with negative position.
 */
void wlr_surface_get_extents(struct wlr_surface *surface, struct wlr_box *box);

/**
 * Get the struct wlr_surface corresponding to a wl_surface resource.
 *
 * This asserts that the resource is a valid wl_surface resource created by
 * wlroots and will never return NULL.
 */
struct wlr_surface *wlr_surface_from_resource(struct wl_resource *resource);

/**
 * Call `iterator` on each mapped surface in the surface tree (whether or not
 * this surface is mapped), with the surface's position relative to the root
 * surface. The function is called from root to leaves (in rendering order).
 */
void wlr_surface_for_each_surface(struct wlr_surface *surface,
	wlr_surface_iterator_func_t iterator, void *user_data);

/**
 * Get the effective surface damage in surface-local coordinate space.
 */
void wlr_surface_get_effective_damage(struct wlr_surface *surface,
	pixman_region32_t *damage);

/**
 * Get the source rectangle describing the region of the buffer that needs to
 * be sampled to render this surface's current state. The box is in
 * buffer-local coordinates.
 *
 * If the viewport's source rectangle is unset, the position is zero and the
 * size is the buffer's.
 */
void wlr_surface_get_buffer_source_box(struct wlr_surface *surface,
	struct wlr_fbox *box);

/**
 * Acquire a lock for the pending surface state.
 *
 * The state won't be committed before the caller releases the lock. Instead,
 * the state becomes cached. The caller needs to use wlr_surface_unlock_cached()
 * to release the lock.
 *
 * Returns a surface commit sequence number for the cached state.
 */
uint32_t wlr_surface_lock_pending(struct wlr_surface *surface);

/**
 * Release a lock for a cached state.
 *
 * Callers should not assume that the cached state will immediately be
 * committed. Another caller may still have an active lock.
 */
void wlr_surface_unlock_cached(struct wlr_surface *surface, uint32_t seq);

/**
 * Set the preferred buffer scale for the surface.
 *
 * This sends an event to the client indicating the preferred scale to use for
 * buffers attached to this surface.
 */
void wlr_surface_set_preferred_buffer_scale(struct wlr_surface *surface,
	int32_t scale);

/**
 * Set the preferred buffer transform for the surface.
 *
 * This sends an event to the client indicating the preferred transform to use
 * for buffers attached to this surface.
 */
void wlr_surface_set_preferred_buffer_transform(struct wlr_surface *surface,
	enum wl_output_transform transform);

struct wlr_surface_synced;

/**
 * Implementation for struct wlr_surface_synced.
 *
 * struct wlr_surface takes care of allocating the sync'ed object state.
 *
 * The only mandatory field is state_size.
 */
struct wlr_surface_synced_impl {
	// Size in bytes of the state struct.
	size_t state_size;
	// Initialize a state. If NULL, this is a no-op.
	void (*init_state)(void *state);
	// Finish a state. If NULL, this is a no-op.
	void (*finish_state)(void *state);
	// Move a state. If NULL, memcpy() is used.
	void (*move_state)(void *dst, void *src);

	// Called when the state is committed. If NULL, this is a no-op.
	// If an object is a surface role object which has state synchronized with
	// the surface state, the role commit hook should be preferred over this.
	void (*commit)(struct wlr_surface_synced *synced);
};

/**
 * An object synchronized with a surface.
 *
 * This is typically used by surface add-ons which integrate with the surface
 * commit mechanism.
 *
 * A sync'ed object maintains state whose lifecycle is managed by
 * struct wlr_surface_synced_impl. Clients make requests to mutate the pending
 * state, then clients commit the pending state via wl_surface.commit. The
 * pending state may become cached, then becomes current when it's applied.
 */
struct wlr_surface_synced {
	struct wlr_surface *surface;
	const struct wlr_surface_synced_impl *impl;
	struct wl_list link; // wlr_surface.synced
	size_t index;
};

/**
 * Initialize a sync'ed object.
 *
 * pending and current must be pointers to the sync'ed object's state. This
 * function will initialize them.
 */
bool wlr_surface_synced_init(struct wlr_surface_synced *synced,
	struct wlr_surface *surface, const struct wlr_surface_synced_impl *impl,
	void *pending, void *current);
/**
 * Finish a sync'ed object.
 *
 * This must be called before the struct wlr_surface is destroyed.
 */
void wlr_surface_synced_finish(struct wlr_surface_synced *synced);
/**
 * Obtain a sync'ed object state.
 */
void *wlr_surface_synced_get_state(struct wlr_surface_synced *synced,
	const struct wlr_surface_state *state);

/**
 * Get a Pixman region from a wl_region resource.
 */
const pixman_region32_t *wlr_region_from_resource(struct wl_resource *resource);

/**
 * Create the wl_compositor global, which can be used by clients to create
 * surfaces and regions.
 *
 * If a renderer is supplied, the compositor will create struct wlr_texture
 * objects from client buffers on surface commit.
 */
struct wlr_compositor *wlr_compositor_create(struct wl_display *display,
	uint32_t version, struct wlr_renderer *renderer);

/**
 * Set the renderer used for creating struct wlr_texture objects from client
 * buffers on surface commit.
 *
 * The renderer may be NULL, in which case no textures are created.
 *
 * Calling this function does not update existing textures, it only affects
 * future surface commits.
 */
void wlr_compositor_set_renderer(struct wlr_compositor *compositor,
	struct wlr_renderer *renderer);

#endif
