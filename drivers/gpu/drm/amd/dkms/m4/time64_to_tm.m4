dnl #
dnl # v4.7-rc2-4-ge6c2682a1da3
dnl # time: Add time64_to_tm()
dnl #
AC_DEFUN([AC_AMDGPU_TIME64_TO_TM], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/time.h>
		], [
			time64_to_tm(0, 0, NULL);
		], [time64_to_tm],[kernel/time/timeconv.c], [
			AC_DEFINE(HAVE_TIME64_TO_TM, 1,
				[time64_to_tm() is available])

		])
	])
])
