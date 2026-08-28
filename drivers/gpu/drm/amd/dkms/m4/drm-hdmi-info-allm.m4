dnl #
dnl # drm/edid: parse HDMI 2.1 gaming (ALLM/VRR) capabilities from HF-VSDB
dnl # v7.1-2754-g0505751e5019
dnl #
AC_DEFUN([AC_AMDGPU_DRM_HDMI_INFO_ALLM], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_connector.h>
		], [
			struct drm_hdmi_info *hdmi = NULL;
			hdmi->allm = false;
		], [
			AC_DEFINE(HAVE_DRM_HDMI_INFO_ALLM, 1,
				[drm_hdmi_info has allm])
		])
	])
])
