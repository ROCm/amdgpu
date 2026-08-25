dnl #
dnl # commit v7.2-rc2-1225-ge11640b9cea4e
dnl # cgroup/dmem: Introduce struct dmem_cgroup_init for region initialization
dnl #
dnl # Newer kernels changed the prototype from
dnl #   dmem_cgroup_register_region(u64 size, const char *name_fmt, ...)
dnl # to
dnl #   dmem_cgroup_register_region(const struct dmem_cgroup_init *init,
dnl #                               const char *name_fmt, ...)
dnl # Detect the new form via struct dmem_cgroup_init.
dnl #
AC_DEFUN([AC_AMDGPU_STRUCT_DMEM_CGROUP_INIT], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/cgroup_dmem.h>
		], [
			struct dmem_cgroup_init init = { 0 };

			init.size = 0;
		], [
			AC_DEFINE(HAVE_STRUCT_DMEM_CGROUP_INIT, 1,
				[struct dmem_cgroup_init is available])
		])
	])
])
