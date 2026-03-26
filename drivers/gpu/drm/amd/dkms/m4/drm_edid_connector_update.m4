dnl #
dnl # commit v5.19-rc2-378-gb71c0aaab9ef
dnl # drm/edid: add drm_edid_connector_update()
dnl #
AC_DEFUN([AC_DRM_EDID_CONNECTOR_UPDATE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_connector_update(NULL, NULL);
		], [drm_edid_connector_update], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_CONNECTOR_UPDATE, 1,
			[drm_edid_connector_update is available])
		])
	])
])
