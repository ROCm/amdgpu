dnl #
dnl # v5.15-rc1-293-g334f74ee85dc
dnl # drm/connector: Add a drm_connector privacy-screen helper functions (v2)
dnl #
AC_DEFUN([AC_AMDGPU_DRM_CONNECTOR_UPDATE_PRIVACY_SCREEN], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <drm/drm_connector.h>
                ], [
                        struct drm_connector_state *drm_con_state = NULL;
                        drm_connector_update_privacy_screen(drm_con_state);
                ], [
                        AC_DEFINE(HAVE_DRM_CONNECTOR_UPDATE_PRIVACY_SCREEN, 1,
                                [drm_connector_update_privacy_screen() is available])
                ])
        ])
])
