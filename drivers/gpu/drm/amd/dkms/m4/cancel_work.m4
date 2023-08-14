dnl #
dnl # v5.18-rc5-2003-g73b4b53276a1
dnl # Revert "workqueue: remove unused cancel_work()"
dnl #
dnl #
AC_DEFUN([AC_AMDGPU_CANCEL_WORK], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/workqueue.h>
		], [
			cancel_work(NULL);
		], [cancel_work], [kernel/workqueue.c], [
			AC_DEFINE(HAVE_CANCEL_WORK, 1,
				[cancel_work() is available])
		])
	])
])
