dnl #
dnl # drm: Rename struct drm_atomic_state to drm_atomic_commit
dnl #
AC_DEFUN([AC_AMDGPU_STRUCT_DRM_ATOMIC_COMMIT], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_atomic.h>
		],[
			struct drm_atomic_commit *state = NULL;
			state->dev = NULL;
		],[
			AC_DEFINE(HAVE_STRUCT_DRM_ATOMIC_COMMIT, 1,
				[struct drm_atomic_state has been renamed to struct drm_atomic_commit])
		])
	])
])
