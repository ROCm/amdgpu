dnl #
dnl # v7.1-rc1-52a9e9cd181fa
dnl # mm: rename zap_vma_ptes() to zap_special_vma_range()
dnl #
AC_DEFUN([AC_AMDGPU_ZAP_SPECIAL_VMA_RANGE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/mm.h>
		], [
			zap_special_vma_range(NULL, 0, 0);
		], [zap_special_vma_range], [mm/memory.c], [
			AC_DEFINE(HAVE_ZAP_SPECIAL_VMA_RANGE, 1,
				[zap_special_vma_range() is available])
		])
	])
])
