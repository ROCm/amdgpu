dnl #
dnl # v5.17-rc3-2-ge1be43d9b5d0
dnl # overflow: Implement size_t saturating arithmetic helpers
dnl #
AC_DEFUN([AC_AMDGPU_SIZE_MUL], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/overflow.h>
		], [
			size_t offset;
			offset = size_mul(0, 0);
		], [
			AC_DEFINE(HAVE_SIZE_MUL, 1,
				[size_mul() is available])
		])
	])
])

AC_DEFUN([AC_AMDGPU_RANGE_OVERFLOWS], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/overflow.h>
		], [
			bool ret;
			ret = range_overflows(0, 0, 0);
		], [
			AC_DEFINE(HAVE_RANGE_OVERFLOWS, 1,
				[range_overflows() is available])
		])
	])
])