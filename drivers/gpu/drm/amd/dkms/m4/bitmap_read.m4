dnl #
dnl # commit v6.9-rc1-324-g63c15822b8dd
dnl # lib/bitmap: add bitmap_{read,write}()
dnl #
AC_DEFUN([AC_AMDGPU_BITMAP_READ], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <linux/bitmap.h>
                ],[
                        bitmap_read(NULL, 0, 0);
                ],[
                        AC_DEFINE(HAVE_BITMAP_READ, 1,
                                [bitmap_read() is available])
                ])
        ])
])
