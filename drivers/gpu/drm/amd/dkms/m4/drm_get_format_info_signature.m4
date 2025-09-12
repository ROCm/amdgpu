dnl #
dnl # v6.16-rc2-1235-g0389e4256eb2
dnl # drm: Pass pixel_format+modifier to .get_format_info()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_GET_FORMAT_INFO_2_ARGS], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_mode_config.h>
			#include <drm/drm_framebuffer.h>
		], [
			const struct drm_format_info *test_get_format_info_new(
				u32 pixel_format, u64 modifier) {
				return NULL;
			}

			struct drm_mode_config_funcs funcs = {
				.get_format_info = test_get_format_info_new,
			};
		], [
			AC_DEFINE(HAVE_DRM_GET_FORMAT_INFO_TWO_ARGS, 1,
				[get_format_info uses pixel_format and modifier parameters])
		])
	])
])