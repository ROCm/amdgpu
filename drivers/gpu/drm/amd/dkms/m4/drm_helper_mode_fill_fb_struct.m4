dnl #
dnl # commit v6.16-rc2-1239-ga34cc7bf1034
dnl # drm: Allow the caller to pass in the format info to drm_helper_mode_fill_fb_struct()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_HELPER_MODE_FILL_FB_STRUCT], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_modeset_helper.h>
			#include <linux/stddef.h>
		],[
			drm_helper_mode_fill_fb_struct(NULL, NULL, NULL, NULL);
		],[drm_helper_mode_fill_fb_struct], [drivers/gpu/drm/drm_modeset_helper.c],[
			AC_DEFINE(HAVE_DRM_HELPER_MODE_FILL_FB_STRUCT_4_ARGS, 1,
				[drm_helper_mode_fill_fb_struct() wants 4 arguments])
		])
	])
])