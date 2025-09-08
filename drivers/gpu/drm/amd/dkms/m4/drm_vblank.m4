dnl #
dnl # v6.10-rc1-595-g0d5040e406d2
dnl # drm/vblank: add dynamic per-crtc vblank configuration support
dnl #
AC_DEFUN([AC_AMDGPU_DRM_VBLANK_CRTC_STRUCT_CONFIG], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <drm/drm_vblank.h>
                ],[
                        struct drm_vblank_crtc *vblank = NULL;
                        struct drm_vblank_crtc_config config;
                        vblank->config = config;
                ],[
                        AC_DEFINE(HAVE_DRM_VBLANK_CRTC_STRUCT_CONFIG, 1,
                                [struct vblank->config is available])
                ])
        ])
])
