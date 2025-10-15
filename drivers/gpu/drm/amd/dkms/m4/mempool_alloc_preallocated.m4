dnl #
dnl # commit v6.7-rc4-323-g37dcc69ad17a
dnl # mempool: introduce mempool_use_prealloc_only
dnl #
AC_DEFUN([AC_AMDGPU_MEMPOOL_ALLOC_PREALLOCATED], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/mempool.h>
		], [
			mempool_alloc_preallocated(NULL);
		],[mempool_alloc_preallocated], [mm/mempool.c],[
			AC_DEFINE(HAVE_MEMPOOL_ALLOC_PREALLOCATED, 1,
			[mempool_alloc_preallocated() is available])
		])
	])
])
 