dnl #
dnl # v6.18-rc2-1333-g621c45ca12ed
dnl # drm: Add Enhanced LUT precision structure
dnl #
AC_DEFUN([AC_AMDGPU_STRUCT_DRM_COLOR_LUT32], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <uapi/drm/drm_mode.h>
        ], [
            struct drm_color_lut32 lut32;
            lut32.red = 0;
        ], [
            AC_DEFINE(HAVE_STRUCT_DRM_COLOR_LUT32, 1,
                [struct drm_color_lut32 is available])
        ])
    ])
])

dnl #
dnl # v6.18-rc2-1333-g621c45ca12ed
dnl # drm: Add Enhanced LUT precision structure
dnl #
AC_DEFUN([AC_AMDGPU_DRM_COLOR_LUT32_SIZE], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <drm/drm_color_mgmt.h>
        ], [
            drm_color_lut32_size(NULL);
        ], [
            AC_DEFINE(HAVE_DRM_COLOR_LUT32_SIZE, 1,
                [drm_color_lut32_size() is available])
        ])
    ])
])

dnl #
dnl # v6.18-rc2-1334-g94529775135c
dnl # drm: Add helper to extract lut from struct drm_color_lut32
dnl #
AC_DEFUN([AC_AMDGPU_DRM_COLOR_LUT32_EXTRACT], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <drm/drm_color_mgmt.h>
        ], [
            drm_color_lut32_extract(0, 0);
        ], [
            AC_DEFINE(HAVE_DRM_COLOR_LUT32_EXTRACT, 1,
                [drm_color_lut32_extract() is available])
        ])
    ])
])

AC_DEFUN([AC_AMDGPU_DRM_COLOR_LUT32], [
    AC_AMDGPU_STRUCT_DRM_COLOR_LUT32
    AC_AMDGPU_DRM_COLOR_LUT32_SIZE
    AC_AMDGPU_DRM_COLOR_LUT32_EXTRACT
])
