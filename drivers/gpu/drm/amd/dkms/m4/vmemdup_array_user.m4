dnl #
dnl # commit v6.6-rc5-1-g313ebe47d755
dnl # kernel: string.h: add array-wrappers for (v)memdup_user()
dnl #
AC_DEFUN([AC_AMDGPU_VMEMDUP_ARRAY_USER], [
     AC_KERNEL_DO_BACKGROUND([
            AC_KERNEL_TRY_COMPILE([
                #include <linux/string.h>
            ], [
                vmemdup_array_user(NULL, 0, 0);
            ], [
                AC_DEFINE(HAVE_VMEMDUP_ARRAY_USER, 1,
                    [vmemdup_array_user() is available])
            ])
    ])
])
