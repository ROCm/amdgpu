#ifndef __AMDGPU_BACKPORT_KCL_DRM_COLOR_MGMT_H__
#define __AMDGPU_BACKPORT_KCL_DRM_COLOR_MGMT_H__

#include <drm/drm_color_mgmt.h>

#ifndef HAVE_STRUCT_DRM_COLOR_LUT32
struct drm_color_lut32 {
	__u32 red;
	__u32 green;
	__u32 blue;
	__u32 reserved;
};
#endif

#ifndef HAVE_DRM_COLOR_LUT32_SIZE
static inline int drm_color_lut32_size(const struct drm_property_blob *blob)
{
	return blob->length / sizeof(struct drm_color_lut32);
}
#endif

#ifndef HAVE_DRM_COLOR_LUT32_EXTRACT
static inline u32 drm_color_lut32_extract(u32 user_input, int bit_precision)
{
	u64 max = (bit_precision >= 64) ? ~0ULL : (1ULL << bit_precision) - 1;

	return DIV_ROUND_CLOSEST_ULL((u64)user_input * max,
				     (1ULL << 32) - 1);
}
#endif

#endif