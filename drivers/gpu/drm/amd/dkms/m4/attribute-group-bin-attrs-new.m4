dnl #
dnl # commit v6.12-rc6-31-g906c508afdca
dnl # sysfs: attribute_group: allow registration of const bin_attribute
dnl #
AC_DEFUN([AC_AMDGPU_ATTRIBUTE_GROUP_BIN_ATTRS_NEW], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/sysfs.h>
		], [
			struct attribute_group group;
			group.bin_attrs_new = NULL;
		], [
			AC_DEFINE(HAVE_ATTRIBUTE_GROUP_BIN_ATTRS_NEW, 1,
				[attribute_group.bin_attrs_new is available])
		])
	])
])
