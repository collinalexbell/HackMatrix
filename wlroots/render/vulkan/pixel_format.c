#include <drm_fourcc.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <wlr/util/log.h>
#include <xf86drm.h>
#include "render/pixel_format.h"
#include "render/vulkan.h"

static const struct wlr_vk_format formats[] = {
	// Vulkan non-packed 8-bits-per-channel formats have an inverted channel
	// order compared to the DRM formats, because DRM format channel order
	// is little-endian while Vulkan format channel order is in memory byte
	// order.
	{
		.drm = DRM_FORMAT_R8,
		.vk = VK_FORMAT_R8_UNORM,
		.vk_srgb = VK_FORMAT_R8_SRGB,
	},
	{
		.drm = DRM_FORMAT_GR88,
		.vk = VK_FORMAT_R8G8_UNORM,
		.vk_srgb = VK_FORMAT_R8G8_SRGB,
	},
	{
		.drm = DRM_FORMAT_RGB888,
		.vk = VK_FORMAT_B8G8R8_UNORM,
		.vk_srgb = VK_FORMAT_B8G8R8_SRGB,
	},
	{
		.drm = DRM_FORMAT_BGR888,
		.vk = VK_FORMAT_R8G8B8_UNORM,
		.vk_srgb = VK_FORMAT_R8G8B8_SRGB,
	},
	{
		.drm = DRM_FORMAT_XRGB8888,
		.vk = VK_FORMAT_B8G8R8A8_UNORM,
		.vk_srgb = VK_FORMAT_B8G8R8A8_SRGB,
	},
	{
		.drm = DRM_FORMAT_XBGR8888,
		.vk = VK_FORMAT_R8G8B8A8_UNORM,
		.vk_srgb = VK_FORMAT_R8G8B8A8_SRGB,
	},
	// The Vulkan _SRGB formats correspond to unpremultiplied alpha, but
	// the Wayland protocol specifies premultiplied alpha on electrical values
	{
		.drm = DRM_FORMAT_ARGB8888,
		.vk = VK_FORMAT_B8G8R8A8_UNORM,
	},
	{
		.drm = DRM_FORMAT_ABGR8888,
		.vk = VK_FORMAT_R8G8B8A8_UNORM,
	},
	// Vulkan packed formats have the same channel order as DRM formats on
	// little endian systems.
#if WLR_LITTLE_ENDIAN
	{
		.drm = DRM_FORMAT_RGBA4444,
		.vk = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_RGBX4444,
		.vk = VK_FORMAT_R4G4B4A4_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_BGRA4444,
		.vk = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_BGRX4444,
		.vk = VK_FORMAT_B4G4R4A4_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_RGB565,
		.vk = VK_FORMAT_R5G6B5_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_BGR565,
		.vk = VK_FORMAT_B5G6R5_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_RGBA5551,
		.vk = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_RGBX5551,
		.vk = VK_FORMAT_R5G5B5A1_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_BGRA5551,
		.vk = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_BGRX5551,
		.vk = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_ARGB1555,
		.vk = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_XRGB1555,
		.vk = VK_FORMAT_A1R5G5B5_UNORM_PACK16,
	},
	{
		.drm = DRM_FORMAT_ARGB2101010,
		.vk = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
	},
	{
		.drm = DRM_FORMAT_XRGB2101010,
		.vk = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
	},
	{
		.drm = DRM_FORMAT_ABGR2101010,
		.vk = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	},
	{
		.drm = DRM_FORMAT_XBGR2101010,
		.vk = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
	},
#endif

	// Vulkan 16-bits-per-channel formats have an inverted channel order
	// compared to DRM formats, just like the 8-bits-per-channel ones.
	// On little endian systems the memory representation of each channel
	// matches the DRM formats'.
#if WLR_LITTLE_ENDIAN
	{
		.drm = DRM_FORMAT_ABGR16161616,
		.vk = VK_FORMAT_R16G16B16A16_UNORM,
	},
	{
		.drm = DRM_FORMAT_XBGR16161616,
		.vk = VK_FORMAT_R16G16B16A16_UNORM,
	},
	{
		.drm = DRM_FORMAT_ABGR16161616F,
		.vk = VK_FORMAT_R16G16B16A16_SFLOAT,
	},
	{
		.drm = DRM_FORMAT_XBGR16161616F,
		.vk = VK_FORMAT_R16G16B16A16_SFLOAT,
	},
#endif

	// YCbCr formats
	// R -> V, G -> Y, B -> U
	// 420 -> 2x2 subsampled, 422 -> 2x1 subsampled, 444 -> non-subsampled
	{
		.drm = DRM_FORMAT_UYVY,
		.vk = VK_FORMAT_B8G8R8G8_422_UNORM,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_YUYV,
		.vk = VK_FORMAT_G8B8G8R8_422_UNORM,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_NV12,
		.vk = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_NV16,
		.vk = VK_FORMAT_G8_B8R8_2PLANE_422_UNORM,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_YUV420,
		.vk = VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_YUV422,
		.vk = VK_FORMAT_G8_B8_R8_3PLANE_422_UNORM,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_YUV444,
		.vk = VK_FORMAT_G8_B8_R8_3PLANE_444_UNORM,
		.is_ycbcr = true,
	},
	// 3PACK16 formats split the memory in three 16-bit words, so they have an
	// inverted channel order compared to DRM formats.
#if WLR_LITTLE_ENDIAN
	{
		.drm = DRM_FORMAT_P010,
		.vk = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_P210,
		.vk = VK_FORMAT_G10X6_B10X6R10X6_2PLANE_422_UNORM_3PACK16,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_P012,
		.vk = VK_FORMAT_G12X4_B12X4R12X4_2PLANE_420_UNORM_3PACK16,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_P016,
		.vk = VK_FORMAT_G16_B16R16_2PLANE_420_UNORM,
		.is_ycbcr = true,
	},
	{
		.drm = DRM_FORMAT_Q410,
		.vk = VK_FORMAT_G10X6_B10X6_R10X6_3PLANE_444_UNORM_3PACK16,
		.is_ycbcr = true,
	},
#endif
	// TODO: add DRM_FORMAT_NV24/VK_FORMAT_G8_B8R8_2PLANE_444_UNORM (requires
	// Vulkan 1.3 or VK_EXT_ycbcr_2plane_444_formats)
};

const struct wlr_vk_format *vulkan_get_format_list(size_t *len) {
	*len = sizeof(formats) / sizeof(formats[0]);
	return formats;
}

const struct wlr_vk_format *vulkan_get_format_from_drm(uint32_t drm_format) {
	for (unsigned i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
		if (formats[i].drm == drm_format) {
			return &formats[i];
		}
	}
	return NULL;
}

const VkImageUsageFlags vulkan_render_usage =
	VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
const VkImageUsageFlags vulkan_shm_tex_usage =
	VK_IMAGE_USAGE_SAMPLED_BIT |
	VK_IMAGE_USAGE_TRANSFER_DST_BIT |
	VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
const VkImageUsageFlags vulkan_dma_tex_usage =
	VK_IMAGE_USAGE_SAMPLED_BIT |
	VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

static const VkFormatFeatureFlags render_features =
	VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
	VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
static const VkFormatFeatureFlags shm_tex_features =
	VK_FORMAT_FEATURE_TRANSFER_SRC_BIT |
	VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
	VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
	// NOTE: we don't strictly require this, we could create a NEAREST
	// sampler for formats that need it, in case this ever makes problems.
	VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
static const VkFormatFeatureFlags dma_tex_features =
	VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
	// NOTE: we don't strictly require this, we could create a NEAREST
	// sampler for formats that need it, in case this ever makes problems.
	VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
static const VkFormatFeatureFlags ycbcr_tex_features =
	VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT |
	VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT;

// vk_format_variant should be set to 0=VK_FORMAT_UNDEFINED when not used
static bool query_modifier_usage_support(struct wlr_vk_device *dev, VkFormat vk_format,
		VkFormat vk_format_variant, VkImageUsageFlags usage,
		const VkDrmFormatModifierPropertiesEXT *m,
		struct wlr_vk_format_modifier_props *out, const char **errmsg) {
	VkResult res;
	*errmsg = NULL;

	VkFormat view_formats[2] = {
		vk_format,
		vk_format_variant,
	};
	VkImageFormatListCreateInfoKHR listi = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
		.pViewFormats = view_formats,
		.viewFormatCount = vk_format_variant ? 2 : 1,
	};
	VkPhysicalDeviceImageDrmFormatModifierInfoEXT modi = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT,
		.drmFormatModifier = m->drmFormatModifier,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.pNext = &listi,
	};
	VkPhysicalDeviceExternalImageFormatInfo efmti = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO,
		.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
		.pNext = &modi,
	};
	VkPhysicalDeviceImageFormatInfo2 fmti = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		.type = VK_IMAGE_TYPE_2D,
		.format = vk_format,
		.usage = usage,
		.flags = vk_format_variant ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0,
		.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT,
		.pNext = &efmti,
	};

	VkExternalImageFormatProperties efmtp = {
		.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES,
	};
	VkImageFormatProperties2 ifmtp = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
		.pNext = &efmtp,
	};
	const VkExternalMemoryProperties *emp = &efmtp.externalMemoryProperties;

	res = vkGetPhysicalDeviceImageFormatProperties2(dev->phdev, &fmti, &ifmtp);
	if (res != VK_SUCCESS) {
		if (res == VK_ERROR_FORMAT_NOT_SUPPORTED) {
			*errmsg = "unsupported format";
		} else {
			wlr_vk_error("vkGetPhysicalDeviceImageFormatProperties2", res);
			*errmsg = "failed to get format properties";
		}
		return false;
	} else if (!(emp->externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT)) {
		*errmsg = "import not supported";
		return false;
	}

	VkExtent3D me = ifmtp.imageFormatProperties.maxExtent;
	*out = (struct wlr_vk_format_modifier_props){
		.props = *m,
		.max_extent.width = me.width,
		.max_extent.height = me.height,
	};
	return true;
}

static bool query_shm_support(struct wlr_vk_device *dev, VkFormat vk_format,
		VkFormat vk_format_variant, VkImageFormatProperties *out,
		const char **errmsg) {
	VkResult res;
	*errmsg = NULL;

	VkFormat view_formats[2] = {
		vk_format,
		vk_format_variant,
	};
	VkImageFormatListCreateInfoKHR listi = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO_KHR,
		.pViewFormats = view_formats,
		.viewFormatCount = vk_format_variant ? 2 : 1,
		.pNext = NULL,
	};
	VkPhysicalDeviceImageFormatInfo2 fmti = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2,
		.type = VK_IMAGE_TYPE_2D,
		.format = vk_format,
		.tiling = VK_IMAGE_TILING_OPTIMAL,
		.usage = vulkan_shm_tex_usage,
		.flags = vk_format_variant ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT : 0,
		.pNext = &listi,
	};
	VkImageFormatProperties2 ifmtp = {
		.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2,
	};

	res = vkGetPhysicalDeviceImageFormatProperties2(dev->phdev, &fmti, &ifmtp);
	if (res != VK_SUCCESS) {
		if (res == VK_ERROR_FORMAT_NOT_SUPPORTED) {
			*errmsg = "unsupported format";
		} else {
			wlr_vk_error("vkGetPhysicalDeviceImageFormatProperties2", res);
			*errmsg = "failed to get format properties";
		}
		return false;
	}

	*out = ifmtp.imageFormatProperties;
	return true;
}

static bool query_modifier_support(struct wlr_vk_device *dev,
		struct wlr_vk_format_props *props, size_t modifier_count) {
	VkDrmFormatModifierPropertiesListEXT modp = {
		.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
		.drmFormatModifierCount = modifier_count,
	};
	VkFormatProperties2 fmtp = {
		.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
		.pNext = &modp,
	};

	modp.pDrmFormatModifierProperties =
		calloc(modifier_count, sizeof(*modp.pDrmFormatModifierProperties));
	if (!modp.pDrmFormatModifierProperties) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		return false;
	}

	vkGetPhysicalDeviceFormatProperties2(dev->phdev, props->format.vk, &fmtp);

	props->dmabuf.render_mods =
		calloc(modp.drmFormatModifierCount, sizeof(*props->dmabuf.render_mods));
	props->dmabuf.texture_mods =
		calloc(modp.drmFormatModifierCount, sizeof(*props->dmabuf.texture_mods));
	if (!props->dmabuf.render_mods || !props->dmabuf.texture_mods) {
		wlr_log_errno(WLR_ERROR, "Allocation failed");
		free(modp.pDrmFormatModifierProperties);
		free(props->dmabuf.render_mods);
		free(props->dmabuf.texture_mods);
		props->dmabuf.render_mods = NULL;
		props->dmabuf.texture_mods = NULL;
		return false;
	}

	bool found = false;
	for (uint32_t i = 0; i < modp.drmFormatModifierCount; ++i) {
		VkDrmFormatModifierPropertiesEXT m = modp.pDrmFormatModifierProperties[i];
		char render_status[256], texture_status[256];

		// check that specific modifier for render usage
		const char *errmsg = "unknown error";
		if ((m.drmFormatModifierTilingFeatures & render_features) == render_features &&
				!props->format.is_ycbcr) {
			struct wlr_vk_format_modifier_props p = {0};
			bool supported = false;
			if (query_modifier_usage_support(dev, props->format.vk,
					props->format.vk_srgb, vulkan_render_usage, &m, &p, &errmsg)) {
				supported = true;
				p.has_mutable_srgb = props->format.vk_srgb != 0;
			}
			if (!supported && props->format.vk_srgb) {
				supported = query_modifier_usage_support(dev, props->format.vk,
					0, vulkan_render_usage, &m, &p, &errmsg);
			}

			if (supported) {
				props->dmabuf.render_mods[props->dmabuf.render_mod_count++] = p;
				wlr_drm_format_set_add(&dev->dmabuf_render_formats,
					props->format.drm, m.drmFormatModifier);
				found = true;
			}
		} else {
			errmsg = "missing required features";
		}
		if (errmsg != NULL) {
			snprintf(render_status, sizeof(render_status), "✗ render (%s)", errmsg);
		} else {
			snprintf(render_status, sizeof(render_status), "✓ render");
		}

		// check that specific modifier for texture usage
		errmsg = "unknown error";
		VkFormatFeatureFlags features = dma_tex_features;
		if (props->format.is_ycbcr) {
			features |= ycbcr_tex_features;
		}
		if ((m.drmFormatModifierTilingFeatures & features) == features) {
			struct wlr_vk_format_modifier_props p = {0};
			bool supported = false;
			if (query_modifier_usage_support(dev, props->format.vk,
					props->format.vk_srgb, vulkan_dma_tex_usage, &m, &p, &errmsg)) {
				supported = true;
				p.has_mutable_srgb = props->format.vk_srgb != 0;
			}
			if (!supported && props->format.vk_srgb) {
				supported = query_modifier_usage_support(dev, props->format.vk,
					0, vulkan_dma_tex_usage, &m, &p, &errmsg);
			}

			if (supported) {
				props->dmabuf.texture_mods[props->dmabuf.texture_mod_count++] = p;
				wlr_drm_format_set_add(&dev->dmabuf_texture_formats,
					props->format.drm, m.drmFormatModifier);
				found = true;
			}
		} else {
			errmsg = "missing required features";
		}
		if (errmsg != NULL) {
			snprintf(texture_status, sizeof(texture_status), "✗ texture (%s)", errmsg);
		} else {
			snprintf(texture_status, sizeof(texture_status), "✓ texture");
		}

		char *modifier_name = drmGetFormatModifierName(m.drmFormatModifier);
		wlr_log(WLR_DEBUG, "    DMA-BUF modifier %s "
			"(0x%016"PRIX64", %"PRIu32" planes): %s  %s",
			modifier_name ? modifier_name : "<unknown>", m.drmFormatModifier,
			m.drmFormatModifierPlaneCount, texture_status, render_status);
		free(modifier_name);
	}

	free(modp.pDrmFormatModifierProperties);
	return found;
}

void vulkan_format_props_query(struct wlr_vk_device *dev,
		const struct wlr_vk_format *format) {
	if (format->is_ycbcr && !dev->sampler_ycbcr_conversion) {
		return;
	}

	char *format_name = drmGetFormatName(format->drm);
	wlr_log(WLR_DEBUG, "  %s (0x%08"PRIX32")",
		format_name ? format_name : "<unknown>", format->drm);
	free(format_name);

	VkDrmFormatModifierPropertiesListEXT modp = {
		.sType = VK_STRUCTURE_TYPE_DRM_FORMAT_MODIFIER_PROPERTIES_LIST_EXT,
	};
	VkFormatProperties2 fmtp = {
		.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2,
		.pNext = &modp,
	};

	vkGetPhysicalDeviceFormatProperties2(dev->phdev, format->vk, &fmtp);

	bool add_fmt_props = false;
	struct wlr_vk_format_props props = {0};
	props.format = *format;

	const struct wlr_pixel_format_info *format_info = drm_get_pixel_format_info(format->drm);

	// shm texture properties
	char shm_texture_status[256];
	const char *errmsg = "unknown error";
	if ((fmtp.formatProperties.optimalTilingFeatures & shm_tex_features) == shm_tex_features &&
			!format->is_ycbcr && format_info != NULL) {
		VkImageFormatProperties ifmtp;
		bool supported = false, has_mutable_srgb = false;
		if (query_shm_support(dev, format->vk, format->vk_srgb, &ifmtp, &errmsg)) {
			supported = true;
			has_mutable_srgb = format->vk_srgb != 0;
		}
		if (!supported && format->vk_srgb) {
			supported = query_shm_support(dev, format->vk, 0, &ifmtp, &errmsg);
		}

		if (supported) {
			props.shm.max_extent.width = ifmtp.maxExtent.width;
			props.shm.max_extent.height = ifmtp.maxExtent.height;
			props.shm.features = fmtp.formatProperties.optimalTilingFeatures;
			props.shm.has_mutable_srgb = has_mutable_srgb;

			wlr_drm_format_set_add(&dev->shm_texture_formats,
				format->drm, DRM_FORMAT_MOD_LINEAR);

			add_fmt_props = true;
		}
	} else {
		errmsg = "missing required features";
	}

	if (errmsg != NULL) {
		snprintf(shm_texture_status, sizeof(shm_texture_status), "✗ texture (%s)", errmsg);
	} else {
		snprintf(shm_texture_status, sizeof(shm_texture_status), "✓ texture");
	}
	wlr_log(WLR_DEBUG, "    Shared memory: %s", shm_texture_status);

	if (modp.drmFormatModifierCount > 0) {
		add_fmt_props |= query_modifier_support(dev, &props,
			modp.drmFormatModifierCount);
	}

	if (add_fmt_props) {
		dev->format_props[dev->format_prop_count] = props;
		++dev->format_prop_count;
	} else {
		vulkan_format_props_finish(&props);
	}
}

void vulkan_format_props_finish(struct wlr_vk_format_props *props) {
	free(props->dmabuf.texture_mods);
	free(props->dmabuf.render_mods);
}

const struct wlr_vk_format_modifier_props *vulkan_format_props_find_modifier(
		const struct wlr_vk_format_props *props, uint64_t mod, bool render) {
	uint32_t len;
	const struct wlr_vk_format_modifier_props *mods;
	if (render) {
		len = props->dmabuf.render_mod_count;
		mods = props->dmabuf.render_mods;
	} else {
		len = props->dmabuf.texture_mod_count;
		mods = props->dmabuf.texture_mods;
	}

	for (uint32_t i = 0; i < len; ++i) {
		if (mods[i].props.drmFormatModifier == mod) {
			return &mods[i];
		}
	}
	return NULL;
}
