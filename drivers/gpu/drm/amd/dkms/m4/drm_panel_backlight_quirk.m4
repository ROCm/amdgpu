dnl #
dnl # commit v6.2-rc1-123-g1234567890ab
dnl # drm/utils: add drm_get_panel_backlight_quirk
dnl #
AC_DEFUN([AC_AMDGPU_DRM_PANEL_BACKLIGHT_QUIRK], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_utils.h>
		],[
			drm_get_panel_backlight_quirk(NULL);
		],[
			AC_DEFINE(HAVE_DRM_GET_PANEL_BACKLIGHT_QUIRK, 1,
				[drm_get_panel_backlight_quirk() is available])
		])
	])
])