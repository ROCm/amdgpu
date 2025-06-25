dnl #
dnl # commit v6.13-rc6-2-g7b0af165e2d4
dnl # drm/drv: Add drmm managed registration helper for dmem cgroups.
dnl #
AC_DEFUN([AC_AMDGPU_DRMM_CGROUP_REGISTER_REGION], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_drv.h>
		], [
			drmm_cgroup_register_region(NULL,NULL,0);
		],[drmm_cgroup_register_region], [drivers/gpu/drm/drm_drv.c],[
			AC_DEFINE(HAVE_DRMM_CGROUP_REGISTER_REGION, 1,
				[drmm_cgroup_register_region() is available])
		])
	])
])
