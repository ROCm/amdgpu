dnl #
dnl # commit v6.2-rc6-1265-g849ee8a2f0df
dnl # drm/suballoc: Extract amdgpu_sa.c as generic suballocation helper
dnl #
AC_DEFUN([AC_AMDGPU_DRM_SUBALLOC_MANAGER_INIT], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <drm/drm_suballoc.h>
                ], [
                        #if IS_ENABLED(CONFIG_DRM_SUBALLOC_HELPER)
			drm_suballoc_manager_init(NULL, 0, 0);
                        #else
                        #error CONFIG_DRM_SUBALLOC_HELPER is not enabled
                        #endif
                ], [
                        AC_DEFINE(HAVE_DRM_SUBALLOC_MANAGER_INIT, 1,
                                [Has function drm_suballoc_manager_init()])
                ])
        ])
])

