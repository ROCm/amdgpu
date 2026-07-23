dnl #
dnl # This file tests block device open APIs across kernel versions.
dnl #
dnl # v6.8+
dnl # block: bdev_file_open_by_path returns struct file *, use file_bdev() to get bdev
dnl #
AC_DEFUN([AC_AMDGPU_BDEV_FILE_OPEN_BY_PATH], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/blkdev.h>
		], [
			struct file *f;
			struct block_device *bdev;
			f = bdev_file_open_by_path("/dev/null", BLK_OPEN_READ, NULL, NULL);
			bdev = file_bdev(f);
			fput(f);
		], [
			AC_DEFINE(HAVE_BDEV_FILE_OPEN_BY_PATH, 1,
				[bdev_file_open_by_path() exists (kernel 6.8+)])
		])
	])
])

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
dnl # Kernel <6.5 (3-arg version)
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
				[blkdev_get_by_path() exists with 3 args (kernel <6.5)])
		])
	])
])

dnl #
dnl # v6.5 with 4-arg blkdev_get_by_path (RHEL backport)
dnl # commit 0718afd47f70c added blk_holder_ops *hops parameter
dnl # blkdev_put() also changed: takes holder instead of mode
dnl # Some enterprise kernels (RHEL 9.4) backport this without bdev_open_by_path
dnl #
AC_DEFUN([AC_AMDGPU_BLKDEV_GET_BY_PATH_4ARG], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/blkdev.h>
		], [
			struct block_device *bdev;
			bdev = blkdev_get_by_path("/dev/null", FMODE_READ, NULL, NULL);
			blkdev_put(bdev, NULL);
		], [
			AC_DEFINE(HAVE_BLKDEV_GET_BY_PATH_4ARG, 1,
				[blkdev_get_by_path() exists with 4 args (RHEL backport)])
		])
	])
])
