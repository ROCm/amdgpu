dnl #
dnl # commit v6.2-rc2-707-gc533b5167c7e
dnl # drm/edid: add separate drm_edid_connector_add_modes()
dnl #
AC_DEFUN([AC_DRM_EDID_CONNECTOR_ADD_MODES], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_connector_add_modes(NULL);
		], [drm_edid_connector_add_modes], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_CONNECTOR_ADD_MODES, 1,
			[drm_edid_connector_add_modes is available])
		]),
	])
])
