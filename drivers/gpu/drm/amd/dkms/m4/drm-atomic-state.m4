dnl #
dnl # v5.0-rc1-415-g022debad063e
dnl # drm/atomic: Add drm_atomic_state->duplicated
dnl #
AC_DEFUN([AC_AMDGPU_STRUCT_DRM_ATOMIC_STATE_DUPLICATED], [
	AC_KERNEL_DO_BACKGROUND([
		dnl # Try new name (drm_atomic_commit) first
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_atomic.h>
		],[
			struct drm_atomic_commit *state = NULL;
			state->duplicated = 0;
		],[
			AC_DEFINE(HAVE_STRUCT_DRM_ATOMIC_STATE_DUPLICATED, 1,
				[struct drm_atomic_state/drm_atomic_commit->duplicated is available])
		])
		dnl # Try old name (drm_atomic_state) for older kernels
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_atomic.h>
		],[
			struct drm_atomic_state *state = NULL;
			state->duplicated = 0;
		],[
			AC_DEFINE(HAVE_STRUCT_DRM_ATOMIC_STATE_DUPLICATED, 1,
				[struct drm_atomic_state/drm_atomic_commit->duplicated is available])
		])
	])
])

dnl #
dnl # drm/atomic: Add drm_atomic_state->plane_color_pipeline
dnl #
AC_DEFUN([AC_AMDGPU_STRUCT_DRM_ATOMIC_STATE_PLANE_COLOR_PIPELINE], [
	AC_KERNEL_DO_BACKGROUND([
		dnl # Try new name (drm_atomic_commit) first
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_atomic.h>
		],[
			struct drm_atomic_commit *state = NULL;
			state->plane_color_pipeline = 0;
		],[
			AC_DEFINE(HAVE_STRUCT_DRM_ATOMIC_STATE_PLANE_COLOR_PIPELINE, 1,
				[struct drm_atomic_state/drm_atomic_commit->plane_color_pipeline is available])
		])
		dnl # Try old name (drm_atomic_state) for older kernels
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_atomic.h>
		],[
			struct drm_atomic_state *state = NULL;
			state->plane_color_pipeline = 0;
		],[
			AC_DEFINE(HAVE_STRUCT_DRM_ATOMIC_STATE_PLANE_COLOR_PIPELINE, 1,
				[struct drm_atomic_state/drm_atomic_commit->plane_color_pipeline is available])
		])
	])
])
