dnl #
dnl # commit v6.13-rc1-117-gcb2e1c2136f7
dnl # drm: remove driver date from struct drm_driver and all drivers
dnl #
dnl # v4.9-rc4-834-g85e634bce01a
dnl # drm: Extract drm_drv.h
dnl #
AC_DEFUN([AC_AMDGPU_DRM_DRIVER_DATE], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <linux/mm_types.h>
            #include <drm/drm_drv.h>
        ],[
            struct drm_driver *drm_driver = NULL;
            drm_driver->date = NULL;
        ],[
            AC_DEFINE(HAVE_DRM_DRIVER_DATE, 1,
                [drm_driver->date is available])
        ])
    ])
])
