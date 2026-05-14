dnl #
dnl # v7.0-rc3-662-gf3934e12f20c5
dnl # drm/connector: Introduce drm_output_color_format enum
dnl #
AC_DEFUN([AC_AMDGPU_DRM_OUTPUT_COLOR_FORMAT], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_connector.h>
		], [
			int fmt = DRM_OUTPUT_COLOR_FORMAT_YCBCR444;
			(void)fmt;
		], [
			AC_DEFINE(HAVE_DRM_OUTPUT_COLOR_FORMAT, 1,
				[enum drm_output_color_format is available])
		])
	])
])
