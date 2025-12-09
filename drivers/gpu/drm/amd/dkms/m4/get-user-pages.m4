dnl #
dnl # commit v6.4-rc4-53-g54d020692b34
dnl # mm/gup: remove unused vmas parameter from get_user_pages()
dnl #
AC_DEFUN([AC_AMDGPU_GET_USER_PAGES], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/mm.h>
		], [
			get_user_pages(0, 0, 0, NULL);
		], [get_user_pages], [mm/gup.c], [
			AC_DEFINE(HAVE_GET_USER_PAGES_REMOVE_VMAS, 1,
				[get_user_pages() remove vmas argument])
		])
	])
])
