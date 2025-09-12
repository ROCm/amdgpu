dnl #
dnl # commit v6.15-rc1-5-g7b73c12c6ebf
dnl # shmem: Add shmem_writeout()
dnl #
AC_DEFUN([AC_AMDGPU_SHMEM_WRITEOUT], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/shmem_fs.h>
		], [
			shmem_writeout(NULL, NULL);
		],[shmem_writeout], [mm/shmem.c],[
			AC_DEFINE(HAVE_SHMEM_WRITEOUT, 1,
				[shmem_writeout() is available])
		],[
			dnl #
			dnl # commit v6.16-rc5-20-g44b1b073eb36
			dnl # mm: stop passing a writeback_control structure to shmem_writeout
			dnl #
			AC_KERNEL_TRY_COMPILE_SYMBOL([
				#include <linux/shmem_fs.h>
			], [
				shmem_writeout(NULL, NULL,NULL);
			],[shmem_writeout], [mm/shmem.c],[
				AC_DEFINE(HAVE_SHMEM_WRITEOUT_WANT_3_ARGS, 1,
					[shmem_writeout() wants 3 arguments])
			])
		])
	])
])
