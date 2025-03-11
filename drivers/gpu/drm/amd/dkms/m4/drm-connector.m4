dnl #
dnl # v6.1-rc1-146-g90b575f52c6a
dnl # drm/edid: detach debugfs EDID override from EDID property update
dnl #
AC_DEFUN([AC_AMDGPU_DRM_CONNECTOR_EDID_OVERRIDE], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <drm/drm_connector.h>
                ],[
			struct drm_connector *connector = NULL;
			connector->edid_override = NULL;
                ],[
                        AC_DEFINE(HAVE_DRM_CONNECTOR_EDID_OVERRIDE, 1,
                                [drm_connector->edid_override is available])
                ])
        ])
])

dnl #
dnl # v6.13-rc2-179-gdf7c8e3dde37
dnl # drm/connector: add mutex to protect ELD from concurrent access
dnl #
AC_DEFUN([AC_AMDGPU_DRM_CONNECTOR_ELD_MUTEX], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <drm/drm_connector.h>
        ],[
            struct drm_connector connector;
            void *ptr = &connector.eld_mutex;
        ],[
            AC_DEFINE(HAVE_DRM_CONNECTOR_ELD_MUTEX, 1,
                [drm_connector->eld_mutex is available])
        ])
    ])
])

AC_DEFUN([AC_AMDGPU_DRM_CONNECTOR_STRUCT_MEMBER], [
    AC_AMDGPU_DRM_CONNECTOR_EDID_OVERRIDE
    AC_AMDGPU_DRM_CONNECTOR_ELD_MUTEX
])

dnl #
dnl # v6.13-rc2-201-g1d985ddabbe0
dnl # drm/connector: Add a way to init/add a connector in separate steps
dnl #
AC_DEFUN([AC_AMDGPU_DRM_CONNECTOR_DYNAMIC_INIT], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <drm/drm_connector.h>
        ],[
            drm_connector_dynamic_init(NULL, NULL, NULL, 0, NULL);
        ],[
            AC_DEFINE(HAVE_DRM_CONNECTOR_DYNAMIC_INIT, 1,
                [drm_connector_dynamic_init is available])
        ])
    ])
])
