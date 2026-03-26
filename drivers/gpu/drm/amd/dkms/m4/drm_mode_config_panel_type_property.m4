dnl #
dnl # v6.19-1925-g227b955a0144
dnl # drm/connector: Add a new 'panel_type' property
dnl #
AC_DEFUN([AC_AMDGPU_STRUCT_DRM_MODE_CONFIG_HAS_PANEL_TYPE_PROPERTY], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_mode_config.h>
		], [
			struct drm_mode_config config;
			config.panel_type_property = 0;
		], [
			AC_DEFINE(HAVE_STRUCT_DRM_MODE_CONFIG_PANEL_TYPE_PROPERTY, 1,
				[struct drm_mode_config has member panel_type_property])
		])
	])
])
