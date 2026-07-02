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

dnl #
dnl # drm: Introduce IN_FORMATS_ASYNC plane property
dnl # v6.19-3092-gab97fcd81bc2
dnl #
AC_DEFUN([AC_AMDGPU_DRM_PLANE_FUNCS_FORMAT_MOD_SUPPORTED_ASYNC], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_plane.h>
		],[
			struct drm_plane_funcs *funcs = NULL;
			funcs->format_mod_supported_async = NULL;
		],[
			AC_DEFINE(HAVE_DRM_PLANE_FUNCS_FORMAT_MOD_SUPPORTED_ASYNC, 1,
				[drm_plane_funcs->format_mod_supported_async is available])
		])
	])
])

dnl #
dnl # commit v6.7-rc1-35ed38d58257
dnl # drm: Allow drivers to indicate the damage helpers to ignore damage clips
dnl # drm_plane_state.ignore_damage_clips
dnl #
AC_DEFUN([AC_AMDGPU_DRM_PLANE_STATE_IGNORE_DAMAGE_CLIPS], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_plane.h>
		],[
			struct drm_plane_state *state = NULL;
			state->ignore_damage_clips = false;
		],[
			AC_DEFINE(HAVE_DRM_PLANE_STATE_IGNORE_DAMAGE_CLIPS, 1,
				[drm_plane_state->ignore_damage_clips is available])
		])
	])
])

AC_DEFUN([AC_AMDGPU_STRUCT_DRM_PLANE], [
	AC_AMDGPU_DRM_PLANE_COLOR_PIPELINE_PROPERTY
	AC_AMDGPU_DRM_PLANE_STATE_COLOR_MGMT_CHANGED
	AC_AMDGPU_DRM_PLANE_FUNCS_FORMAT_MOD_SUPPORTED_ASYNC
	AC_AMDGPU_DRM_PLANE_STATE_IGNORE_DAMAGE_CLIPS
])