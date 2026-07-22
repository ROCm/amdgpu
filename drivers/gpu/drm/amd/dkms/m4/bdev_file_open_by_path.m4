dnl #
dnl # This file tests block device open APIs across kernel versions.
dnl # Despite the filename, it tests both the new (6.5+) and old (<6.5) APIs.
dnl #
dnl # v6.5-rc1
dnl # block: Use bdev_open_by_path instead of blkdev_get_by_path
dnl # bdev_open_by_path returns struct bdev_handle *, use bdev_release() to release
dnl #
AC_DEFUN([AC_AMDGPU_BDEV_OPEN_BY_PATH], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/blkdev.h>
		], [
			struct bdev_handle *handle;
			struct block_device *bdev;
			handle = bdev_open_by_path("/dev/null", BLK_OPEN_READ, NULL, NULL);
			bdev = handle->bdev;
			bdev_release(handle);
		], [
			AC_DEFINE(HAVE_BDEV_OPEN_BY_PATH, 1,
				[bdev_open_by_path() exists and returns struct bdev_handle *])
		])
	])
])

dnl #
dnl # Kernel <6.5
dnl # block: blkdev_get_by_path returns struct block_device *
dnl #
AC_DEFUN([AC_AMDGPU_BLKDEV_GET_BY_PATH], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/blkdev.h>
		], [
			struct block_device *bdev;
			bdev = blkdev_get_by_path("/dev/null", FMODE_READ, NULL);
			blkdev_put(bdev, FMODE_READ);
		], [
			AC_DEFINE(HAVE_BLKDEV_GET_BY_PATH, 1,
				[blkdev_get_by_path() exists (kernel <6.5)])
		])
	])
])
