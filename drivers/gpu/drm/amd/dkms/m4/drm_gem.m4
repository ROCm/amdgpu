dnl #
dnl # v6.14-rc4-395-gb57aa47d39e9
dnl # drm/gem: Test for imported GEM buffers with helper
dnl #
AC_DEFUN([AC_AMDGPU_DRM_GEM_IS_IMPORTED], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <drm/drm_gem.h>
                ],[
                        drm_gem_is_imported(NULL);
                ],[
                    AC_DEFINE(HAVE_DRM_GEM_IS_IMPORTED, 1,
                            [drm_gem_is_imported() is available])
                ])
        ])
])
