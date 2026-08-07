dnl #
dnl # commit 998427b3ad2c7
dnl # mm/notifier: add migration invalidation type
dnl # add MMU_NOTIFY_MIGRATE to enum mmu_notifier_event
dnl #
AC_DEFUN([AC_AMDGPU_MMU_NOTIFY_MIGRATE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/mmu_notifier.h>
		], [
			enum mmu_notifier_event event = MMU_NOTIFY_MIGRATE;
			(void)event;
		], [
			AC_DEFINE(HAVE_MMU_NOTIFY_MIGRATE, 1,
				[MMU_NOTIFY_MIGRATE is available])
		])
	])
])
