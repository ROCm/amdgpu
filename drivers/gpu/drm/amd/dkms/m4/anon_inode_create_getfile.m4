dnl #
dnl # v6.8 commit 4f0b9194bc11 ("fs: Rename anon_inode_getfile_secure() and
dnl # anon_inode_getfd_secure()"), exported by a7800aa80ea4
dnl #
AC_DEFUN([AC_AMDGPU_ANON_INODE_CREATE_GETFILE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/anon_inodes.h>
		], [
			struct file *f;
			f = anon_inode_create_getfile("kcl", NULL, NULL, 0, NULL);
		], [anon_inode_create_getfile], [fs/anon_inodes.c], [
			AC_DEFINE(HAVE_ANON_INODE_CREATE_GETFILE, 1,
				[anon_inode_create_getfile() is exported])
		])
	])
])
