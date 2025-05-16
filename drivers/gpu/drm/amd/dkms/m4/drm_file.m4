dnl #
dnl # v6.12-rc1-309-g56c594d8df64
dnl #
AC_DEFUN([AC_AMDGPU_DRM_FILE_CLIENT_NAME], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/drm_file.h>
		], [
			struct drm_file *filp = NULL;
			filp->client_name = NULL;
		], [
			AC_DEFINE(HAVE_DRM_FILE_CLIENT_NAME, 1,
				[struct drm_file->client_name is available])
		])
	])
])