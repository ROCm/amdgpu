dnl #
dnl # commit v5.2-rc5-7-g5c73deb23b11
dnl # ktime: Provide us_to_ktime() helper
dnl #
AC_DEFUN([AC_AMDGPU_US_TO_KTIME], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/ktime.h>
		], [
			ktime_t t = us_to_ktime(100);
			(void)t;
		], [
			AC_DEFINE(HAVE_US_TO_KTIME, 1,
				[us_to_ktime() is available])
		])
	])
])
