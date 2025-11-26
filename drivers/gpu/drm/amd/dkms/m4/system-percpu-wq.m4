dnl #
dnl # commit id: v6.15-1-g128ea9f6ccfb
dnl # workqueue: Add system_percpu_wq and system_dfl_wq
dnl #
AC_DEFUN([AC_AMDGPU_SYSTEM_PERCPU_WQ], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/workqueue.h>
		], [
			struct workqueue_struct *wq = system_percpu_wq;
		], [system_percpu_wq], [kernel/workqueue.c], [
			AC_DEFINE(HAVE_SYSTEM_PERCPU_WQ, 1,
				[system_percpu_wq is available])
		])
	])
])