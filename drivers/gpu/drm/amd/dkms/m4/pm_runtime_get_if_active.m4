dnl #
dnl # commit v5.6-rc4-1-gc111566bea7c
dnl # PM: runtime: Add pm_runtime_get_if_active()
dnl #
AC_DEFUN([AC_AMDGPU_PM_RUNTIME_GET_IF_ACTIVE], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <linux/pm_runtime.h>
        ],[
            pm_runtime_get_if_active(NULL);
        ],[
            AC_DEFINE(HAVE_PM_RUNTIME_GET_IF_ACTIVE_1ARGS, 1,
                [pm_runtime_get_if_active() has one parameters])
        ],[
            AC_KERNEL_TRY_COMPILE([
                #include <linux/pm_runtime.h>
            ],[
                pm_runtime_get_if_active(NULL, 0);
            ],[
                AC_DEFINE(HAVE_PM_RUNTIME_GET_IF_ACTIVE_2ARGS, 1,
                    [pm_runtime_get_if_active() has two parameters])
            ])
        ])
    ])
])
