dnl #
dnl # v5.12-rc7-1527-g98025a62cb00
dnl # drm/dp_mst: Use Extended Base Receiver Capability DPCD space
dnl #
AC_DEFUN([AC_AMDGPU_DRM_DP_MST_TOPOLOGY_MGR_INIT], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_dp_mst_helper.h>
		], [
			drm_dp_mst_topology_mgr_init(NULL, (struct drm_device *)NULL, NULL, 0, 0, 0, 0, 0);
		], [
			AC_DEFINE(HAVE_DRM_DP_MST_TOPOLOGY_MGR_INIT_MAX_LANE_COUNT, 1,
				[drm_dp_mst_topology_mgr_init() has max_lane_count and max_link_rate])

		])
	])
])
