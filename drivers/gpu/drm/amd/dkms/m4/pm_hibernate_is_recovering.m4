dnl #
dnl # 23994a6f084303bbd5948973cca0ea4ee9694ad8
dnl # PM: hibernate: add new api pm_hibernate_is_recovering()
dnl #
AC_DEFUN([AC_AMDGPU_PM_HIBERNATE_IS_RECOVERING], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/suspend.h>
		],[
			pm_hibernate_is_recovering();
		],[
			AC_DEFINE(HAVE_PM_HIBERNATE_IS_RECOVERING,
				1,
				[pm_hibernate_is_recovering() is available])
		])
	])
])