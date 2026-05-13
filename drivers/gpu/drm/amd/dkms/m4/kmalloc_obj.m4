dnl #
dnl # v7.0-rc1-g2932ba8d9c99
dnl # slab: Introduce kmalloc_obj() and family
dnl #
AC_DEFUN([AC_AMDGPU_KMALLOC_OBJ], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/slab.h>
		], [
			struct { int x; } *p;
			p = kmalloc_obj(*p);
		], [
			AC_DEFINE(HAVE_KMALLOC_OBJ, 1,
				[kmalloc_obj() and related typed allocation macros are available])
		])
	])
])
