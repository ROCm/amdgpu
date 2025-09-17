dnl #
dnl # commit v5.16-23-g0425473037db
dnl # list: introduce list_is_head() helper and re-use it in list.h
dnl #
AC_DEFUN([AC_AMDGPU_LIST_IS_HEAD], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/list.h>
		], [
			list_is_head(NULL, NULL);
		], [
			AC_DEFINE(HAVE_LIST_IS_HEAD, 1,
				[list_is_head() is available])
		])
	])
])
