dnl #
dnl # v6.14-rc6-302-g82ba975e4c43
dnl # mm: allow compound zone device pages
dnl #
AC_DEFUN([AC_AMDGPU_PAGE_PGMAP], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/mmzone.h>
		], [
			page_pgmap(NULL);
		], [
			AC_DEFINE(HAVE_PAGE_PGMAP, 1,
				[page_pgmap() is available])
		])
	])
])
