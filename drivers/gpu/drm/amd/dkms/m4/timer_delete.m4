dnl #
dnl # commit v6.1-rc1-12-gbb663f0f3c39
dnl # timers: Rename del_timer() to timer_delete()
dnl #
AC_DEFUN([AC_AMDGPU_TIMER_DELETE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/timer.h>
		], [
			timer_delete(NULL);
		],[
			AC_DEFINE(HAVE_TIMER_DELETE, 1,
				[timer_delete() is available])
		])
	])
])
