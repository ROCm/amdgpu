// SPDX-License-Identifier: MIT
#include <kcl/kcl_drm_fourcc.h>

#ifndef HAVE_DRM_FORMAT_INFO_BPP
/*
 * Backport of drm_format_info_bpp() for kernels that lack the helper.
 * Mirrors the upstream implementation in drivers/gpu/drm/drm_fourcc.c using
 * the per-plane block fields available in struct drm_format_info.
 */
unsigned int kcl_drm_format_info_bpp(const struct drm_format_info *info, int plane)
{
	unsigned int block_w, block_h, block_size;

	if (!info || plane < 0 || plane >= info->num_planes)
		return 0;

	block_w = info->block_w[plane] ? info->block_w[plane] : 1;
	block_h = info->block_h[plane] ? info->block_h[plane] : 1;
	block_size = block_w * block_h;

	if (info->char_per_block[plane] * 8 % block_size)
		return 0;

	return info->char_per_block[plane] * 8 / block_size;
}
EXPORT_SYMBOL(kcl_drm_format_info_bpp);
#endif /* HAVE_DRM_FORMAT_INFO_BPP */
