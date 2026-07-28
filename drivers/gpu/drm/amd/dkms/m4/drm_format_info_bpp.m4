dnl #
dnl # v5.14-rc1 commit 1d2f5a3d5df8
dnl # drm: Add bits-per-pixel from drm_format_info to drm_format_info_bpp()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_FORMAT_INFO_BPP], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_fourcc.h>
		], [
			drm_format_info_bpp(NULL, 0);
		], [drm_format_info_bpp], [drivers/gpu/drm/drm_fourcc.c], [
			AC_DEFINE(HAVE_DRM_FORMAT_INFO_BPP, 1,
				[drm_format_info_bpp() is available])
		])
	])
])
