dnl #
dnl # commit fd265d9e0c3358e6
dnl # drm: add drm_memory_stats_is_zero
dnl #
AC_DEFUN([AC_AMDGPU_DRM_MEMORY_STATS_IS_ZERO], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <drm/drm_file.h>
        ], [
            drm_memory_stats_is_zero(NULL);
        ], [
            AC_DEFINE(HAVE_DRM_MEMORY_STATS_IS_ZERO, 1,
                [drm_memory_stats_is_zero() is available])
        ])
    ])
])

dnl #
dnl # commit fd265d9e0c3358e6
dnl # drm: add drm_memory_stats_is_zero
dnl #
AC_DEFUN([AC_AMDGPU_STRUCT_DRM_MEMORY_STATS], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <drm/drm_file.h>
        ], [
            struct drm_memory_stats *stats;
            stats = NULL;
        ], [
            AC_DEFINE(HAVE_STRUCT_DRM_MEMORY_STATS, 1,
                [struct drm_memory_stats is defined])
        ])
    ])
])

AC_DEFUN([AC_AMDGPU_DRM_MEMORY_STATS], [
    AC_AMDGPU_DRM_MEMORY_STATS_IS_ZERO
    AC_AMDGPU_STRUCT_DRM_MEMORY_STATS
])



