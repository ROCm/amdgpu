dnl #
dnl # v6.19-rc7-1691-g47b5ac7daa46e
dnl # drm/atomic: Add new atomic_create_state callback to drm_private_obj
dnl #
AC_DEFUN([AC_AMDGPU_DRM_PRIVATE_STATE_FUNCS_ATOMIC_CREATE_STATE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_atomic.h>
		], [
			struct drm_private_state_funcs *funcs = NULL;
			(void)funcs->atomic_create_state;
		], [
			AC_DEFINE(HAVE_DRM_PRIVATE_STATE_FUNCS_ATOMIC_CREATE_STATE, 1,
				[struct drm_private_state_funcs has atomic_create_state])
		])
	])
])
