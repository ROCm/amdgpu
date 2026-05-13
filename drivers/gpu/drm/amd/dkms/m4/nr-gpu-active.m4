dnl #
dnl # v7.0-rc7-1767-g2232ba9c7931d
dnl # mm: add gpu active/reclaim per-node stat counters
dnl #
AC_DEFUN([AC_AMDGPU_NR_GPU_ACTIVE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/mmzone.h>
		], [
			enum node_stat_item item = NR_GPU_ACTIVE;
			(void)item;
		], [
			AC_DEFINE(HAVE_NR_GPU_ACTIVE, 1, [NR_GPU_ACTIVE is available])
		])
	])
])
