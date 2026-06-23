dnl #
dnl # commit v6.8-rc1-123-gd35b4e13b8b8
dnl # drm/plane: Add color pipeline property
dnl #
AC_DEFUN([AC_AMDGPU_DRM_PLANE_COLOR_PIPELINE_PROPERTY], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_plane.h>
		],[
			struct drm_plane *plane = NULL;
			plane->color_pipeline_property = NULL;
		],[
			AC_DEFINE(HAVE_DRM_PLANE_COLOR_PIPELINE_PROPERTY, 1,
				[drm_plane->color_pipeline_property is available])
		])
	])
])

dnl #
dnl # commit v6.7-rc5-864-g24013b9301349
dnl # drm/drm_plane: track color mgmt changes per plane
dnl #
AC_DEFUN([AC_AMDGPU_DRM_PLANE_STATE_COLOR_MGMT_CHANGED], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_plane.h>
		],[
			struct drm_plane_state *plane_state = NULL;
			plane_state->color_mgmt_changed = 0;
		],[
			AC_DEFINE(HAVE_STRUCT_DRM_PLANE_STATE_COLOR_MGMT_CHANGED, 1,
				[struct drm_plane_state->color_mgmt_changed is available])
		])
	])
])

AC_DEFUN([AC_AMDGPU_STRUCT_DRM_PLANE], [
	AC_AMDGPU_DRM_PLANE_COLOR_PIPELINE_PROPERTY
	AC_AMDGPU_DRM_PLANE_STATE_COLOR_MGMT_CHANGED
])