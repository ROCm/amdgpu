dnl #
dnl # commit v6.16-rc6-1339-g9b934e465aaf
dnl # PM: hibernate: add new api pm_hibernate_is_recovering()
dnl #
AC_DEFUN([AC_AMDGPU_PM_HIBERNATE_IS_RECOVERING], [
     AC_KERNEL_DO_BACKGROUND([
            AC_KERNEL_TRY_COMPILE([
                #include <linux/suspend.h>
            ], [
                pm_hibernate_is_recovering();
            ], [
                AC_DEFINE(HAVE_PM_HIBERNATE_IS_RECOVERING, 1,
                    [pm_hibernate_is_recovering() is available])
            ])
    ])
])

