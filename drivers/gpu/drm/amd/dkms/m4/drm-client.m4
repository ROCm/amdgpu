dnl #
dnl # commit v6.11-rc7-1514-gd07fdf922592
dnl # drm: Add client-agnostic setup helper
dnl #
AC_DEFUN([AC_AMDGPU_DRM_CLIENT_SETUP], [
     AC_KERNEL_DO_BACKGROUND([
            AC_KERNEL_TRY_COMPILE([
                #ifdef  HAVE_DRM_DRM_CLIENT_SETUP_H
                #include <drm/drm_client_setup.h>
                #endif
                #ifdef  HAVE_DRM_CLIENTS_DRM_CLIENT_SETUP_H
                #include <drm/clients/drm_client_setup.h>
                #endif
            ], [
                drm_client_setup(NULL, NULL);
            ], [
                AC_DEFINE(HAVE_DRM_CLIENT_SETUP, 1,
                    [drm_client_setup() is available])
            ])
    ])
])

AC_DEFUN([AC_AMDGPU_DRM_CLIENT], [
    AC_AMDGPU_DRM_CLIENT_SETUP
])
