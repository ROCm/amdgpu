dnl #
dnl # v6.12-rc6-1237-g22e5c7ae1214 drm: Add panel backlight quirks
dnl #
AC_DEFUN([AC_AMDGPU_DRM_GET_PANEL_MIN_BRIGHTNESS_QUIRK], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE_SYMBOL([
                    #include <drm/drm_utils.h>
                ],[
                    drm_get_panel_min_brightness_quirk(NULL);
                ],[drm_get_panel_min_brightness_quirk], [drivers/gpu/drm/drm_panel_backlight_quirks.c], [
                        AC_DEFINE(HAVE_DRM_GET_PANEL_MIN_BRIGHTNESS_QUIRK, 1, [drm_get_panel_min_brightness_quirk() is available])
                ])
        ])
])
