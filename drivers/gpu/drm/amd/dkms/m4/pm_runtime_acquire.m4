dnl #
dnl # v6.19 commit ef8057b07c72 ("PM: runtime: Wrapper macros for
dnl # ACQUIRE()/ACQUIRE_ERR()")
dnl #
AC_DEFUN([AC_AMDGPU_PM_RUNTIME_ACQUIRE], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <linux/pm_runtime.h>
        ],[
            struct device *dev = NULL;

            PM_RUNTIME_ACQUIRE_IF_ENABLED_AUTOSUSPEND(dev, lock);
            (void)PM_RUNTIME_ACQUIRE_ERR(&lock);
        ],[
            AC_DEFINE(HAVE_PM_RUNTIME_ACQUIRE, 1,
                [PM_RUNTIME_ACQUIRE*() macros are available])
        ])
    ])
])
