/*
 * This an unstable interface of wlroots. No guarantees are made regarding the
 * future consistency of this API.
 */
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif

#ifndef WLR_TYPES_WLR_BUFFER_H
#define WLR_TYPES_WLR_BUFFER_H

#include <pixman.h>
#include <wayland-server-core.h>
#include <wlr/render/dmabuf.h>
#include <wlr/util/addon.h>

struct wlr_buffer;
struct wlr_renderer;

/**
 * Shared-memory attributes for a buffer.
 */
struct wlr_shm_attributes {
	int fd;
	uint32_t format; // FourCC code, see DRM_FORMAT_* in <drm_fourcc.h>
	int width, height;
	int stride; // Number of bytes between consecutive pixel lines
	off_t offset; // Offset in bytes of the first pixel in FD
};

/**
 * Buffer capabilities.
 *
 * These bits indicate the features supported by a struct wlr_buffer. There is
 * one bit per function in struct wlr_buffer_impl.
 */
enum wlr_buffer_cap {
	WLR_BUFFER_CAP_DATA_PTR = 1 << 0,
	WLR_BUFFER_CAP_DMABUF = 1 << 1,
	WLR_BUFFER_CAP_SHM = 1 << 2,
};

/**
 * A buffer containing pixel data.
 *
 * A buffer has a single producer (the party who created the buffer) and
 * multiple consumers (parties reading the buffer). When all consumers are done
 * with the buffer, it gets released and can be re-used by the producer. When
 * the producer and all consumers are done with the buffer, it gets destroyed.
 */
struct wlr_buffer {
	const struct wlr_buffer_impl *impl;

	int width, height;

	bool dropped;
	size_t n_locks;
	bool accessing_data_ptr;

	struct {
		struct wl_signal destroy;
		struct wl_signal release;
	} events;

	struct wlr_addon_set addons;
};

/**
 * Unreference the buffer. This function should be called by producers when
 * they are done with the buffer.
 */
void wlr_buffer_drop(struct wlr_buffer *buffer);
/**
 * Lock the buffer. This function should be called by consumers to make
 * sure the buffer can be safely read from. Once the consumer is done with the
 * buffer, they should call wlr_buffer_unlock().
 */
struct wlr_buffer *wlr_buffer_lock(struct wlr_buffer *buffer);
/**
 * Unlock the buffer. This function should be called by consumers once they are
 * done with the buffer.
 */
void wlr_buffer_unlock(struct wlr_buffer *buffer);
/**
 * Reads the DMA-BUF attributes of the buffer. If this buffer isn't a DMA-BUF,
 * returns false.
 *
 * The returned DMA-BUF attributes are valid for the lifetime of the
 * struct wlr_buffer. The caller isn't responsible for cleaning up the DMA-BUF
 * attributes.
 */
bool wlr_buffer_get_dmabuf(struct wlr_buffer *buffer,
	struct wlr_dmabuf_attributes *attribs);
/**
 * Read shared memory attributes of the buffer. If this buffer isn't shared
 * memory, returns false.
 *
 * The returned shared memory attributes are valid for the lifetime of the
 * struct wlr_buffer. The caller isn't responsible for cleaning up the shared
 * memory attributes.
 */
bool wlr_buffer_get_shm(struct wlr_buffer *buffer,
	struct wlr_shm_attributes *attribs);
/**
 * Transforms a struct wl_resource into a struct wlr_buffer and locks it. Once
 * the caller is done with the buffer, they must call wlr_buffer_unlock().
 *
 * The provided struct wl_resource must be a wl_buffer.
 */
struct wlr_buffer *wlr_buffer_try_from_resource(struct wl_resource *resource);

/**
 * Check whether a buffer is fully opaque.
 *
 * When true is returned, the buffer is guaranteed to be fully opaque, but the
 * reverse is not true: false may be returned in cases where the buffer is fully
 * opaque.
 */
bool wlr_buffer_is_opaque(struct wlr_buffer *buffer);

/**
 * Buffer data pointer access flags.
 */
enum wlr_buffer_data_ptr_access_flag {
	/**
	 * The buffer contents can be read back.
	 */
	WLR_BUFFER_DATA_PTR_ACCESS_READ = 1 << 0,
	/**
	 * The buffer contents can be written to.
	 */
	WLR_BUFFER_DATA_PTR_ACCESS_WRITE = 1 << 1,
};

/**
 * Get a pointer to a region of memory referring to the buffer's underlying
 * storage. The format and stride can be used to interpret the memory region
 * contents.
 *
 * The returned pointer should be pointing to a valid memory region for the
 * operations specified in the flags. The returned pointer is only valid up to
 * the next wlr_buffer_end_data_ptr_access() call.
 */
bool wlr_buffer_begin_data_ptr_access(struct wlr_buffer *buffer, uint32_t flags,
	void **data, uint32_t *format, size_t *stride);
/**
 * Indicate that a pointer to a buffer's underlying memory will no longer be
 * used.
 *
 * This function must be called after wlr_buffer_begin_data_ptr_access().
 */
void wlr_buffer_end_data_ptr_access(struct wlr_buffer *buffer);

/**
 * A client buffer.
 */
struct wlr_client_buffer {
	struct wlr_buffer base;

	/**
	 * The buffer's texture, if any. A buffer will not have a texture if the
	 * client destroys the buffer before it has been released.
	 */
	struct wlr_texture *texture;
	/**
	 * The buffer this client buffer was created from. NULL if destroyed.
	 */
	struct wlr_buffer *source;

	struct {
		struct wl_listener source_destroy;
		struct wl_listener renderer_destroy;

		size_t n_ignore_locks;
	} WLR_PRIVATE;
};

/**
 * Get a client buffer from a generic buffer. If the buffer isn't a client
 * buffer, returns NULL.
 */
struct wlr_client_buffer *wlr_client_buffer_get(struct wlr_buffer *buffer);

/**
 * A single-pixel buffer.  Used by clients to draw solid-color rectangles.
 */
struct wlr_single_pixel_buffer_v1 {
	struct wlr_buffer base;

	// Full-scale for each component is UINT32_MAX
	uint32_t r, g, b, a;

	struct {
		struct wl_resource *resource;
		struct wl_listener release;

		// Packed little-endian DRM_FORMAT_ARGB8888. Used for data_ptr_access
		uint8_t argb8888[4];
	} WLR_PRIVATE;
};

/**
 * If the wlr_buffer is a wlr_single_pixel_buffer_v1 then unwrap it.
 * Otherwise, returns NULL.
 */
struct wlr_single_pixel_buffer_v1 *wlr_single_pixel_buffer_v1_try_from_buffer(
	struct wlr_buffer *buffer);

#endif
