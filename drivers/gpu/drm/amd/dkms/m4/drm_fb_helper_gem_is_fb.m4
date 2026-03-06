dnl #
dnl # commit amd-staging-dkms-6.18.10-84-gdeca4d1c0b26
dnl # drm/amdgpu: Move test for fbdev GEM object into generic helper
dnl #
AC_DEFUN([AC_AMDGPU_DRM_FB_HELPER_GEM_IS_FB], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_fb_helper.h>
		], [
			struct drm_fb_helper *fb_helper = NULL;
			struct drm_gem_object *obj = NULL;
			drm_fb_helper_gem_is_fb(fb_helper, obj);
		], [drm_fb_helper_gem_is_fb], [drivers/gpu/drm/drm_fb_helper.c], [
			AC_DEFINE(HAVE_DRM_FB_HELPER_GEM_IS_FB, 1,
			[drm_fb_helper_gem_is_fb is available])
		])
	])
])
