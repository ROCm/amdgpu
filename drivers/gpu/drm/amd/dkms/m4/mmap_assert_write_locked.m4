dnl #
dnl # v5.7-13157-g42fc541404f2
dnl # mmap locking API: add mmap_assert_locked() and mmap_assert_write_locked()
dnl #
AC_DEFUN([AC_AMDGPU_MMAP_ASSERT_WRITE_LOCKED], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <linux/mmap_lock.h>
                ], [
                        mmap_assert_write_locked(NULL);
                ], [
                        AC_DEFINE(HAVE_MMAP_ASSERT_WRITE_LOCKED, 1,
                                [mmap_assert_write_locked() is  available])
                ])
        ])
])

