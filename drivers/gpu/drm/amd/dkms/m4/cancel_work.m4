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

dnl #
dnl # commit id:v5.0-rc2-28-g8204e0c1113d
dnl # workqueue: Provide queue_work_node to queue work near a given NUMA node
dnl #
AC_DEFUN([AC_AMDGPU_QUEUE_WORK_NODE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/workqueue.h>
		], [
			queue_work_node(0,NULL, NULL);
		], [queue_work_node], [kernel/workqueue.c], [
			AC_DEFINE(HAVE_QUEUE_WORK_NODE, 1,
				[queue_work_node() is available])
		])
	])
])

dnl #
dnl # v6.11-rc1-3-g86898fa6b8cd
dnl # workqueue: Implement disable/enable for (delayed) work items
dnl #
AC_DEFUN([AC_AMDGPU_DISABLE_DELAYED_WORK_SYNC], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/workqueue.h>
		], [
			disable_delayed_work_sync(NULL);
		], [disable_delayed_work_sync], [kernel/workqueue.c], [
			AC_DEFINE(HAVE_DISABLE_DELAYED_WORK_SYNC, 1,
				[disable_delayed_work_sync() is available])
		])
	])
])

AC_DEFUN([AC_AMDGPU_WORKQUEUE], [
	AC_AMDGPU_CANCEL_WORK
	AC_AMDGPU_QUEUE_WORK_NODE
	AC_AMDGPU_DISABLE_DELAYED_WORK_SYNC
])
