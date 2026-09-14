dnl #
dnl # v6.4-rc4-4-g3db55767da74
dnl # add intptr_t
dnl #
AC_DEFUN([AC_AMDGPU_TYPE_INTPTR_T], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/types.h>
		], [
			intptr_t i = 0;
		], [
			AC_DEFINE(HAVE_TYPE_INTPTR_T, 1,
				[intptr_t is available])
		])
	])
])
