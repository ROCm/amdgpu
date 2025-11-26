dnl #
dnl # v6.18-rc1-214-g1226cd7c7686
dnl # drm/atomic: Change state pointers to a more meaningful name
dnl #
AC_DEFUN([AC_AMDGPU_STRUCT_DRM_PRIVATE_OBJS_STATE_STATE_TO_DESTROY], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_atomic.h>
		],[
			struct __drm_private_objs_state *obj_state = NULL;
			obj_state->state_to_destroy = NULL;
		],[
			AC_DEFINE(HAVE_STRUCT_DRM_PRIVATE_OBJS_STATE_STATE_TO_DESTROY, 1,
				[struct __drm_private_objs_state->state_to_destroy is available])
		])
	])
])

AC_DEFUN([AC_AMDGPU_STRUCT_DRM_PRIVATE_OBJS_STATE], [
	AC_AMDGPU_STRUCT_DRM_PRIVATE_OBJS_STATE_STATE_TO_DESTROY
])