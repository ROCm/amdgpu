dnl #
dnl # commit v6.15-rc1-7-g97d06802d10a
dnl # sysfs: constify bin_attribute argument of bin_attribute::read/write()
dnl #
AC_DEFUN([AC_AMDGPU_BIN_ATTR_CONST_ARGS], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/sysfs.h>
		], [
			ssize_t (*p)(struct file *, struct kobject *, const struct bin_attribute *,
			char *, loff_t, size_t) = NULL;

			struct bin_attribute bin_attr;
			bin_attr.read = p;
		], [
			AC_DEFINE(HAVE_BIN_ATTR_CONST_ARGS, 1,
				[bin_attribute.read is const args])
		])
	])
])
