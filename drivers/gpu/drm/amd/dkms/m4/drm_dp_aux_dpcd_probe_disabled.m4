dnl #
dnl # commit v6.16-rc1-124-gb87ed522b364
dnl # drm/dp: Add an EDID quirk for the DPCD register access probe
dnl #
AC_DEFUN([AC_AMDGPU_DRM_DP_AUX_DPCD_PROBE_DISABLED], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/display/drm_dp_helper.h>
		], [
			struct drm_dp_aux *aux = NULL;
			aux->dpcd_probe_disabled = false;
		], [
			AC_DEFINE(HAVE_DRM_DP_AUX_DPCD_PROBE_DISABLED, 1,
				[drm_dp_aux->dpcd_probe_disabled is available])
		])
	])
])
