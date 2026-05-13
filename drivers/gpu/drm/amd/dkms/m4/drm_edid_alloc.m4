dnl #
dnl # commit v5.18-rc5-1218-g6537f79a2aae
dnl # drm/edid: add new interfaces around struct drm_edid
dnl #
AC_DEFUN([AC_AMDGPU_DRM_EDID_MALLOC], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_alloc(NULL, 0);
		], [drm_edid_alloc], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_MALLOC, 1,
				[drm_edid_alloc() is available and exported])
		])
	])
])

dnl #
dnl # commit v5.19-rc2-380-g3d1ab66e043f
dnl # drm/edid: add drm_edid_raw() to access the raw EDID data
dnl #
AC_DEFUN([AC_AMDGPU_DRM_EDID_RAW], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_raw(NULL);
		], [drm_edid_raw], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_RAW, 1,
				[drm_edid_raw() is available and exported])
		])
	])
])

dnl #
dnl # commit v6.1-rc1-145-g6c9b3db70aad
dnl # drm/edid: add function for checking drm_edid validity
dnl #
AC_DEFUN([AC_AMDGPU_DRM_EDID_VALID], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_valid(NULL);
		], [drm_edid_valid], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_VALID, 1,
				[drm_edid_valid() is available and exported])
		])
	])
])


dnl #
dnl # commit v6.3-rc2-329-gf1e4c916f97f
dnl # drm/edid: add drm_edid_dup()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_EDID_DUP], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_dup(NULL);
		], [drm_edid_dup], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_DUP, 1,
				[drm_edid_dup() is available and exported])
		])
	])
])

dnl #
dnl # commit v6.3-rc2-336-gce6526ab2364
dnl # drm/edid: add drm_edid_read_ddc()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_EDID_READ_DDC], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_read_ddc(NULL, NULL);
		], [drm_edid_read_ddc], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_READ_DDC, 1,
				[drm_edid_read_ddc() is available and exported])
		])
	])
])

dnl #
dnl # commit v5.20-rc1-474-g5ceb7058b5fe
dnl # drm/edid: add drm_edid_connector_update()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_EDID_CONNECTOR_UPDATE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_connector_update(NULL, NULL);
		], [drm_edid_connector_update], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_CONNECTOR_UPDATE, 1,
				[drm_edid_connector_update() is available and exported])
		])
	])
])

dnl #
dnl # commit v5.20-rc1-479-gedd1f90a8403
dnl # drm/edid: add drm_edid_connector_add_modes()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_EDID_CONNECTOR_ADD_MODES], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_connector_add_modes(NULL);
		], [drm_edid_connector_add_modes], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_CONNECTOR_ADD_MODES, 1,
				[drm_edid_connector_add_modes() is available and exported])
		])
	])
])

dnl #
dnl # commit v6.3-rc2-327-g02364dd57e33
dnl # drm/edid: add drm_edid_is_digital()
dnl #
AC_DEFUN([AC_AMDGPU_DRM_EDID_IS_DIGITAL], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_edid.h>
		], [
			drm_edid_is_digital(NULL);
		], [drm_edid_is_digital], [drivers/gpu/drm/drm_edid.c], [
			AC_DEFINE(HAVE_DRM_EDID_IS_DIGITAL, 1,
				[drm_edid_is_digital() is available and exported])
		])
	])
])

AC_DEFUN([AC_AMDGPU_STRUCT_DRM_EDID], [
	AC_AMDGPU_DRM_EDID_MALLOC
	AC_AMDGPU_DRM_EDID_RAW
	AC_AMDGPU_DRM_EDID_VALID
	AC_AMDGPU_DRM_EDID_DUP
	AC_AMDGPU_DRM_EDID_READ_DDC
	AC_AMDGPU_DRM_EDID_CONNECTOR_UPDATE
	AC_AMDGPU_DRM_EDID_CONNECTOR_ADD_MODES
	AC_AMDGPU_DRM_EDID_IS_DIGITAL
])