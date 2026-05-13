dnl #
dnl # v6.19-rc6-397-gbae0ba7c7c0a0
dnl # mm: add basic VMA flag operation helper functions
dnl #
AC_DEFUN([AC_AMDGPU_EMPTY_VMA_FLAGS], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/mm_types.h>
		], [
			vma_flags_t flags = EMPTY_VMA_FLAGS;
			(void)flags;
		], [
			AC_DEFINE(HAVE_EMPTY_VMA_FLAGS, 1,
				[EMPTY_VMA_FLAGS is available])
		])
	])
])
