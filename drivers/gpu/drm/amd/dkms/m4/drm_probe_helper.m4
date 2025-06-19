dnl #
dnl # v5.19-rc6-1875-g216b9bbaeaea
dnl # drm/probe-helper: Add drm_crtc_helper_mode_valid_fixed()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_CRTC_HELPER_MODE_VALID_FIXED], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_probe_helper.h>
		], [
		    drm_crtc_helper_mode_valid_fixed(NULL, NULL, NULL);
		], [drm_crtc_helper_mode_valid_fixed], [drivers/gpu/drm/drm_probe_helper.c],[
			AC_DEFINE(HAVE_DRM_CRTC_HELPER_MODE_VALID_FIXED, 1,
				[drm_crtc_helper_mode_valid_fixed() is available])
		])
	])
])
