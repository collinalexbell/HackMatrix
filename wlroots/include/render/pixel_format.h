#ifndef RENDER_PIXEL_FORMAT_H
#define RENDER_PIXEL_FORMAT_H

#include <wayland-server-protocol.h>

/**
 * Information about a pixel format.
 *
 * A pixel format is identified via its DRM four character code (see <drm_fourcc.h>).
 *
 * Simple formats have a block size of 1×1 pixels and bytes_per_block contains
 * the number of bytes per pixel (including padding).
 *
 * Tiled formats (e.g. sub-sampled YCbCr) are described with a block size
 * greater than 1×1 pixels. A block is a rectangle of pixels which are stored
 * next to each other in a byte-aligned memory region.
 */
struct wlr_pixel_format_info {
	uint32_t drm_format;

	/* Equivalent of the format if it has an alpha channel,
	 * DRM_FORMAT_INVALID (0) if NA
	 */
	uint32_t opaque_substitute;

	/* Bytes per block (including padding) */
	uint32_t bytes_per_block;
	/* Size of a block in pixels (zero for 1×1) */
	uint32_t block_width, block_height;
};

/**
 * Get pixel format information from a DRM FourCC.
 *
 * NULL is returned if the pixel format is unknown.
 */
const struct wlr_pixel_format_info *drm_get_pixel_format_info(uint32_t fmt);
/**
 * Get the number of pixels per block for a pixel format.
 */
uint32_t pixel_format_info_pixels_per_block(const struct wlr_pixel_format_info *info);
/**
 * Get the minimum stride for a given pixel format and width.
 */
int32_t pixel_format_info_min_stride(const struct wlr_pixel_format_info *info, int32_t width);
/**
 * Check whether a stride is large enough for a given pixel format and width.
 */
bool pixel_format_info_check_stride(const struct wlr_pixel_format_info *info,
	int32_t stride, int32_t width);

/**
 * Convert an enum wl_shm_format to a DRM FourCC.
 */
uint32_t convert_wl_shm_format_to_drm(enum wl_shm_format fmt);
/**
 * Convert a DRM FourCC to an enum wl_shm_format.
 */
enum wl_shm_format convert_drm_format_to_wl_shm(uint32_t fmt);

/**
 * Return true if the DRM FourCC fmt has an alpha channel, false otherwise.
 */
bool pixel_format_has_alpha(uint32_t fmt);

#endif
