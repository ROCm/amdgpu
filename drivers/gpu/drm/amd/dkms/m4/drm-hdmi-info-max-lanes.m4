dnl #
dnl # v5.18-rc1-1-g210713a00a8f
dnl # drm/edid: parse HDMI Forum EDIDD
dnl # struct drm_hdmi_info gained max_lanes, max_frl_rate_per_lane, dsc_cap
dnl #
AC_DEFUN([AC_AMDGPU_DRM_HDMI_INFO_MAX_LANES], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_connector.h>
		], [
			struct drm_hdmi_info *hdmi = NULL;
			hdmi->max_lanes = 0;
		], [
			AC_DEFINE(HAVE_DRM_HDMI_INFO_MAX_LANES, 1,
				[drm_hdmi_info has max_lanes/max_frl_rate_per_lane/dsc_cap])
		])
	])
])
