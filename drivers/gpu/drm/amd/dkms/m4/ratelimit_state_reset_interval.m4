dnl #
dnl # commit v6.15-rc1-1-g56a7b9f8b059
dnl # ratelimit: Create functions to handle ratelimit_state internals
dnl #
AC_DEFUN([AC_AMDGPU_RATELIMIT_STATE_RESET_INTERVAL], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/ratelimit.h>
		], [
			ratelimit_state_reset_interval(NULL,0);
		],[
			AC_DEFINE(HAVE_RATELIMIT_STATE_RESET_INTERVAL, 1,
				[ratelimit_state_reset_interval() is available])
		])
	])
])
