
dnl #
dnl # commit v6.1-rc1~27-c4f306e31632
dnl # drm/amdgpu: use idr_init_base() to initialize fpriv->bo_list_handles
dnl #
AC_DEFUN([AC_AMDGPU_IDR_INIT_BASE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/idr.h>
		], [
			idr_init_base(NULL, 0);
		], [
			AC_DEFINE(HAVE_IDR_INIT_BASE, 1,
				[idr_init_base() is available])
		])
	])
])

dnl #
dnl # commit v4.16-rc1~25-6ce711f27500
dnl # idr: Make 1-based IDRs more efficient
dnl #
AC_DEFUN([AC_AMDGPU_STRUCT_IDE_IDR_BASE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/idr.h>
		], [
			struct idr *idr = NULL;
			idr->idr_base = 0;
		], [
			AC_DEFINE(HAVE_STRUCT_IDE_IDR_BASE, 1,
				[ide->idr_base is available])
		])
	])
])


AC_DEFUN([AC_AMDGPU_IDR], [
	AC_AMDGPU_IDR_INIT_BASE
	AC_AMDGPU_STRUCT_IDE_IDR_BASE
])
