dnl #
dnl # commit v6.12-rc1-119-g908a1d775422
dnl # hrtimers: Introduce hrtimer_setup() to replace hrtimer_init()
dnl #
AC_DEFUN([AC_AMDGPU_HRTIMER_SETUP], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/hrtimer.h>
		], [
			hrtimer_setup(NULL,NULL,0,0);
		],[hrtimer_setup], [kernel/time/hrtimer.c],[
			AC_DEFINE(HAVE_HRTIMER_SETUP, 1,
				[hrtimer_setup() is available])
		])
	])
])
