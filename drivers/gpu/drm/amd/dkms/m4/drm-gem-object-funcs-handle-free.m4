dnl #
dnl # commit 9ac4301d4457
dnl # drm/gem: Add callback for when handle count goes to 0
dnl #
AC_DEFUN([AC_AMDGPU_DRM_GEM_OBJECT_FUNCS_HANDLE_FREE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_gem.h>
		], [
			struct drm_gem_object_funcs *funcs = NULL;
			funcs->handle_free(NULL);
		], [
			AC_DEFINE(HAVE_DRM_GEM_OBJECT_FUNCS_HANDLE_FREE, 1,
				[drm_gem_object_funcs.handle_free is available])
		])
	])
])
