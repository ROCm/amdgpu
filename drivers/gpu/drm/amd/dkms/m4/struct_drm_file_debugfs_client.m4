dnl #
dnl # commit v6.16-rc1-406-g1fd45bc21cec
dnl # drm: add debugfs support on per client-id basis
dnl #
AC_DEFUN([AC_AMDGPU_DRM_FILE_DEBUGFS_CLIENT], [
     AC_KERNEL_DO_BACKGROUND([
            AC_KERNEL_TRY_COMPILE([
                #include <drm/drm_file.h>
            ], [
                struct drm_file file;
                file.debugfs_client = NULL;
            ], [
                AC_DEFINE(STRUCT_DRM_FILE_DEBUGFS_CLIENT, 1,
                    [drm_file->debugfs_client is available])
            ])
    ])
])

