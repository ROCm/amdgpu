dnl #
dnl # commit v6.14-rc1-238-gb7cf9f4ac1b8
dnl # drm: Introduce device wedged event
dnl #
AC_DEFUN([AC_AMDGPU_DRM_DEV_WEDGED_EVENT], [
     AC_KERNEL_DO_BACKGROUND([
            AC_KERNEL_TRY_COMPILE([
                #include <drm/drm_drv.h>
            ], [
                drm_dev_wedged_event(NULL, 0);
            ], [
                AC_DEFINE(HAVE_DRM_DEV_WEDGED_EVENT, 1,
                    [drm_dev_wedged_event() is available])
            ])
    ])
])

