dnl #
dnl # drm/connector: Introduce the "color format" property
dnl # adds enum drm_connector_color_format, drm_connector_state.color_format
dnl # and drm_connector_attach_color_format_property()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_CONNECTOR_COLOR_FORMAT], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_connector.h>
		], [
			struct drm_connector_state *state = NULL;
			state->color_format = DRM_CONNECTOR_COLOR_FORMAT_AUTO;
			drm_connector_attach_color_format_property(NULL, 0);
		], [
			AC_DEFINE(HAVE_DRM_CONNECTOR_COLOR_FORMAT, 1,
				[drm_connector_state->color_format is available])
		])
	])
])
