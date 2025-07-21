dnl #
dnl # commit v6.16-rc1-333-g183bccafa176
dnl #  drm: Create a task info option for wedge events
dnl #
dnl # commit v6.14-rc1-238-gb7cf9f4ac1b8
dnl # drm: Introduce device wedged event
dnl #
AC_DEFUN([AC_AMDGPU_DRM_DEV_WEDGED_EVENT], [
     AC_KERNEL_DO_BACKGROUND([
            AC_KERNEL_TRY_COMPILE_SYMBOL([
                #include <drm/drm_drv.h>
            ], [
                drm_dev_wedged_event(NULL, 0, NULL);
            ], [drm_dev_wedged_event], [drivers/gpu/drm/drm_drv.c], [
                AC_DEFINE(HAVE_DRM_DEV_WEDGED_EVENT, 1,
                    [drm_dev_wedged_event() is available])
            ])
    ])
])

