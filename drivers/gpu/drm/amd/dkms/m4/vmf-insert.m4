dnl #
dnl # commit v6.16-rc5-105-g21aa65bf82a7
dnl # mm: remove callers of pfn_t functionality
dnl #
AC_DEFUN([AC_AMDGPU_VMF_INSERT_MIXED_MKWRITE_LONG_PFN], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/mm.h>
		],[
			vmf_insert_mixed_mkwrite(NULL, 0, 0);
		],[
			AC_DEFINE(HAVE_VMF_INSERT_MIXED_MKWRITE_LONG_PFN, 1,
				[vmf_insert_mixed_mkwrite() has long pfn arg])
		])
	])
])
