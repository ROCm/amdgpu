dnl #
dnl # commit v6.15-rc1-272-g8702048bb831
dnl # mm/kmap: Add kmap_local_page_try_from_panic()
dnl #
AC_DEFUN([AC_AMDGPU_KMAP_LOCAL_PAGE_TRY_FROM_PANIC], [
     AC_KERNEL_DO_BACKGROUND([
            AC_KERNEL_TRY_COMPILE([
                #include <linux/highmem.h>
            ], [
                kmap_local_page_try_from_panic(NULL);
            ], [
                AC_DEFINE(HAVE_KMAP_LOCAL_PAGE_TRY_FROM_PANIC, 1,
                    [kmap_local_page_try_from_panic() is available])
            ])
    ])
])

