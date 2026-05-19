dnl #
dnl # v6.14-rc1-399-g28a4bfa7b48e (drm: Allow driver-managed destruction of colorop objects)
dnl # struct drm_colorop_funcs was added and colorop init functions gained a funcs parameter
dnl #
AC_DEFUN([AC_AMDGPU_DRM_COLOROP_FUNCS], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <drm/drm_colorop.h>
		], [
			struct drm_colorop_funcs funcs = { 0 };
			(void)funcs.destroy;
		], [
			AC_DEFINE(HAVE_DRM_COLOROP_FUNCS, 1,
				[struct drm_colorop_funcs is available])
		])
	])
])
