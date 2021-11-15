dnl #
dnl # commit v5.15-rc2-1075-gd6c6a76f80a1
dnl # Update MST First Link Slot Information Based on Encoding Format
dnl #
AC_DEFUN([AC_AMDGPU_DRM_DP_MST_TOPOLOGY_STATE_TOTAL_AVAIL_SLOTS], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_dp_mst_helper.h>
		], [
			struct drm_dp_mst_topology_state * mst_state = NULL;
			mst_state->total_avail_slots = 0;
		], [
			AC_DEFINE(HAVE_DRM_DP_MST_TOPOLOGY_STATE_TOTAL_AVAIL_SLOTS, 1,
				[struct drm_dp_mst_topology_state has member total_avail_slots])
		])
	])
])

