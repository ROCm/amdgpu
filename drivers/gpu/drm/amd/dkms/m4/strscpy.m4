dnl #
dnl # commit v6.8-rc2-15-ge6584c3964f2
dnl # string: Allow 2-argument strscpy()
dnl #
AC_DEFUN([AC_AMDGPU_STRSCPY_ALLOW_TWO_ARGUMENTS], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/string.h>
			#include <linux/bug.h>
		],[
			char name[8];
			strscpy(name, "foo");
		],[
			AC_DEFINE(HAVE_STRSCPY_ALLOW_TWO_ARGUMENTS, 1,
				[strscpy() allows two arguments])
		])
	])
])
