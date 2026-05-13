dnl #
dnl # v5.14-10450-g4eb6bd55cfb22
dnl # compiler.h: drop fallback overflow checkers
dnl #
AC_DEFUN([AC_AMDGPU_CHECK_SUB_OVERFLOW_BUILTIN], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/overflow.h>
		], [
			u32 a = 10;
			int b = 3;
			u32 d;
			(void)check_sub_overflow(a, b, &d);
		], [
			AC_DEFINE(HAVE_CHECK_SUB_OVERFLOW_BUILTIN, 1,
				[check_sub_overflow uses __builtin_sub_overflow])
		])
	])
])
