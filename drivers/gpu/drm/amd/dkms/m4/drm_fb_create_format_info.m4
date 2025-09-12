dnl #
dnl # commit v6.16-rc2-1238-g81112eaac559
dnl # drm: Pass the format info to .fb_create()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_FB_CREATE_FORMAT_INFO], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_mode_config.h>
			#include <drm/drm_framebuffer.h>
		], [
			struct drm_framebuffer *test_fb_create_new(
				struct drm_device *dev,
				struct drm_file *file_priv,
				const struct drm_format_info *info,
				const struct drm_mode_fb_cmd2 *mode_cmd) {
				return NULL;
			}

			struct drm_mode_config_funcs funcs = {
				.fb_create = test_fb_create_new,
			};
		], [
			AC_DEFINE(HAVE_DRM_FB_CREATE_FORMAT_INFO, 1,
				[fb_create has const struct drm_format_info * parameter])
		])
	])
])
