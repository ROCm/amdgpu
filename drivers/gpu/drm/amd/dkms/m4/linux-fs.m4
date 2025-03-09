dnl #
dnl # commit v6.9-rc1-17-g210a03c9d51a
dnl # fs: claw back a few FMODE_* bits
dnl #
AC_DEFUN([AC_AMDGPU_FILE_OPERATION_FOP_FLAGS], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <linux/fs.h>
        ],[
            struct file_operations file_operation;
            file_operation.fop_flags = 0;
        ],[
            AC_DEFINE(HAVE_FILE_OPERATION_FOP_FLAGS, 1,
                [file_operation->fop_flags is available])
        ])
    ])
])

dnl #
dnl # commit v6.11-rc1-2-g88a2f6468d01
dnl # struct fd: representation change
dnl #
AC_DEFUN([AC_AMDGPU_FD], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <linux/file.h>
        ],[
            struct fd f;
            f.word = 0;
            fd_empty(f);
        ],[
            AC_DEFINE(HAVE_FD_EMPTY, 1,
                [fd_empty is available])
        ])
    ])
])

AC_DEFUN([AC_AMDGPU_STRUCT_FILE_OPERATION], [
    AC_AMDGPU_FILE_OPERATION_FOP_FLAGS
    AC_AMDGPU_FD
])
