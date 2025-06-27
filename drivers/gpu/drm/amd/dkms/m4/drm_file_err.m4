dnl #
dnl # v6.14-1914-gec0a2cc40756
dnl # dma-buf: Add drm_file_err (v1)
dnl #
AC_DEFUN([AC_AMDGPU_DRM_FILE_ERR], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_file.h>
		], [
			drm_file_err(NULL, NULL);
		], [drm_file_err], [drivers/gpu/drm/drm_file.c],[
			AC_DEFINE(HAVE_DRM_FILE_ERR, 1,
				[drm_file_err() is available])
		])
	])
])