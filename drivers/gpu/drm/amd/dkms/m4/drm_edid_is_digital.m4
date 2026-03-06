dnl #
dnl # commit v6.5-rc2-869-g7218779efc46
dnl # drm/edid: add drm_edid_is_digital()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_EDID_IS_DIGITAL], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_edid.h>
		], [
			const struct drm_edid *drm_edid = NULL;
			drm_edid_is_digital(drm_edid);
		], [
			AC_DEFINE(HAVE_DRM_EDID_IS_DIGITAL, 1,
			[drm_edid_is_digital is available])
		])
	])
])
