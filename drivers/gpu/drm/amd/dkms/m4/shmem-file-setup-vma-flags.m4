dnl #
dnl # v7.0-rc4-590d356aa433
dnl # mm: update shmem_[kernel]_file_*() functions to use vma_flags_t
dnl #
AC_DEFUN([AC_AMDGPU_SHMEM_FILE_SETUP_VMA_FLAGS], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/shmem_fs.h>
			#include <linux/mm_types.h>
		], [
			shmem_file_setup("test", 0, EMPTY_VMA_FLAGS);
		], [
			AC_DEFINE(HAVE_SHMEM_FILE_SETUP_VMA_FLAGS, 1,
				[shmem_file_setup() takes vma_flags_t arg])
		])
	])
])
