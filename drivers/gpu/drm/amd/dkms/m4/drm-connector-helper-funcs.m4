dnl #
dnl # commit v5.2-rc2-529-g6f3b62781bbd
dnl # drm: Convert connector_helper_funcs->atomic_check to accept drm_atomic_state
dnl #
AC_DEFUN([AC_AMDGPU_DRM_CONNECTOR_HELPER_FUNCS_ATOMIC_CHECK], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_modeset_helper_vtables.h>
			#include <drm/drm_atomic.h>
		], [
			struct drm_connector_helper_funcs *p = NULL;
			p->atomic_check(NULL, (struct drm_atomic_state*)NULL);
		], [
			AC_DEFINE(HAVE_DRM_CONNECTOR_HELPER_FUNCS_ATOMIC_CHECK_ARG_DRM_ATOMIC_STATE, 1,
				[drm_connector_helper_funcs->atomic_check() wants struct drm_atomic_state arg])
		])
	])
])

dnl #
dnl # v5.10-rc3-1075-geca22edb37d2
dnl # drm: Pass the full state to connectors atomic functions
dnl #
AC_DEFUN([AC_AMDGPU_CONNECTOR_HELPER_FUNCTS_ATOMIC_BEST_ENCODER], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_modeset_helper_vtables.h>
			#include <drm/drm_atomic.h>
		], [
			struct drm_connector_helper_funcs *p = NULL;
			p->atomic_best_encoder(NULL, (struct drm_atomic_state*)NULL);
		], [
			AC_DEFINE(HAVE_DRM_CONNECTOR_HELPER_FUNCS_ATOMIC_BEST_ENCODER_ARG_DRM_ATOMIC_STATE, 1,
				[atomic_best_encoder take 2nd arg type of state as struct drm_atomic_state])
		])
	])
])

dnl #
dnl # v6.13-rc2-288-g26d6fd81916e
dnl # drm/connector: make mode_valid take a const struct drm_display_mode
dnl #
AC_DEFUN([AC_AMDGPU_CONNECTOR_HELPER_FUNCTS_MODE_VALID_CONST_ARGUMENT], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
                #include <drm/drm_modeset_helper_vtables.h>
            ], [
                struct drm_connector_helper_funcs test_funcs = {
                .mode_valid = (enum drm_mode_status (*)(struct drm_connector *, const struct drm_display_mode *))0
            };
            ], [
                AC_DEFINE(HAVE_DRM_CONNECTOR_HELPER_FUNCS_MODE_VALID_CONST_ARGUMENT, 1,
                    [.mode_valid need a const drm_display_mode argument])
            ])
    ])
])

AC_DEFUN([AC_AMDGPU_DRM_CONNECTOR_HELPER_FUNCS], [
	AC_AMDGPU_DRM_CONNECTOR_HELPER_FUNCS_ATOMIC_CHECK
	AC_AMDGPU_CONNECTOR_HELPER_FUNCTS_ATOMIC_BEST_ENCODER
	AC_AMDGPU_CONNECTOR_HELPER_FUNCTS_MODE_VALID_CONST_ARGUMENT
])
