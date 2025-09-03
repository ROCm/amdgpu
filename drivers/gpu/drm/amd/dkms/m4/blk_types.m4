dnl #
dnl # v5.10-rc4-97-g0d02129e76ed
dnl # block: merge struct block_device and struct hd_struct
dnl #
AC_DEFUN([AC_AMDGPU_BLOCK_DEVICE_BD_DEVICE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/blk_types.h>		
		], [
			struct block_device *device;
			device.bd_device = NULL;
		], [],[],[
			AC_DEFINE(HAVE_BLOCK_DEVICE_BD_DEVICE, 1,
				[struct block_device has member named 'bd_device'])
		])
	])
])
