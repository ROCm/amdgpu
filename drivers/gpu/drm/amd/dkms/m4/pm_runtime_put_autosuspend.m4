AC_DEFUN([AC_AMDGPU_PM_RUNTIME_PUT_AUTOSUSPEND], [
    AC_MSG_CHECKING([if pm_runtime_put_autosuspend includes pm_runtime_mark_last_busy])
        AS_IF([sed -n '/static inline int pm_runtime_put_autosuspend/,/^}/p' ${LINUX}/include/linux/pm_runtime.h | grep -q "pm_runtime_mark_last_busy"], [
            AC_MSG_RESULT([yes])
            AC_DEFINE(HAVE_PM_RUNTIME_PUT_AUTOSUSPEND_MARK_LAST_BUSY, 1,
                [pm_runtime_put_autosuspend includes pm_runtime_mark_last_busy])
        ], [
            AC_MSG_RESULT([no])
        ])
])
