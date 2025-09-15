dnl #
dnl # commit: v5.5-rc7-11-g5dcdc43e24a1
dnl # vfs: add vfs_iocb_iter_[read|write] helper functions
dnl #
AC_DEFUN([AC_AMDGPU_VFS_IOCB_ITER_READ], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include<linux/fs.h>
		],[	
			vfs_iocb_iter_read(NULL, NULL, NULL);
		],[vfs_iocb_iter_read],[fs/read_write.c], [
			AC_DEFINE(HAVE_VFS_IOCB_ITER_READ, 1,
			[vfs_iocb_iter_read() is available])
		])
	])
])
