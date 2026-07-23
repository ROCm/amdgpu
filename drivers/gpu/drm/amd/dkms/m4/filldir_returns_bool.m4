dnl #
dnl # Before v6.1-rc1
dnl # fs: filldir_t callback used to return int (changed to bool in 6.1)
dnl # Test for the OLD API (int return type)
dnl #
AC_DEFUN([AC_AMDGPU_FILLDIR_RETURNS_INT], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/fs.h>

			static int test_filldir(struct dir_context *ctx,
						const char *name, int namlen,
						loff_t offset, u64 ino,
						unsigned int d_type)
			{
				return 0;
			}
		], [
			struct dir_context ctx;
			ctx.actor = test_filldir;
		], [
			AC_DEFINE(HAVE_FILLDIR_RETURNS_INT, 1,
				[filldir_t callback returns int (kernel <6.1)])
		])
	])
])
