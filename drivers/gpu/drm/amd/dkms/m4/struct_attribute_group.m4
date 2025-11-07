dnl #
dnl # commit v6.12-rc6-16-gb626816fdd7f
dnl # sysfs: treewide: constify attribute callback of bin_is_visible()
dnl #
AC_DEFUN([AC_AMDGPU_BIN_FLASH_ATTR_IS_VISIBLE], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <linux/sysfs.h>
            #include <linux/types.h>
            static umode_t amdgpu_bin_attr_is_visible(struct kobject *kobj,
                                                const struct bin_attribute *attr,
                                                int idx)
            {
                return 0;
            }
            struct attribute_group amdgpu_attr_group = {
                .is_bin_visible = amdgpu_bin_attr_is_visible,
            };

        ],[
            (void)amdgpu_attr_group;
        ],[
            AC_DEFINE(HAVE_CONSTANT_ARGUMENT_IN_IS_BIN_VISIBLE, 1,
                [need a const argument in member func .is_bin_visible])
        ])
    ])
])

dnl #
dnl # commit v5.10-rc1 or later
dnl # sysfs: Make bin_attrs in struct attribute_group const
dnl #
AC_DEFUN([AC_AMDGPU_ATTRIBUTE_GROUP_BIN_ATTRS_NEW], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/sysfs.h>
		],[
			const struct bin_attribute *const test_bin_attrs[] = { NULL };
			struct attribute_group test_group = {
				.bin_attrs = test_bin_attrs,
			};
			(void)test_group;
		],[
			AC_DEFINE(HAVE_ATTRIBUTE_GROUP_BIN_ATTRS_NEW, 1,
				[bin_attrs in struct attribute_group is const])
		])
	])
])
