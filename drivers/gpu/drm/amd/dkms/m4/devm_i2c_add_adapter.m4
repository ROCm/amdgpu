dnl #
dnl # commit v5.12-rc6-34-g07740c92ae57
dnl # i2c: core: add managed function for adding i2c adapters
dnl #
AC_DEFUN([AC_AMDGPU_DEVM_I2C_ADD_ADAPTER], [
AC_KERNEL_DO_BACKGROUND([
AC_KERNEL_TRY_COMPILE_SYMBOL([
#include <linux/i2c.h>
		], [
			devm_i2c_add_adapter(NULL, NULL);
		],[devm_i2c_add_adapter], [drivers/i2c/i2c-core-base.c],[
			AC_DEFINE(HAVE_DEVM_I2C_ADD_ADAPTER, 1,
			[devm_i2c_add_adapter() is available])
		])
	])
])
