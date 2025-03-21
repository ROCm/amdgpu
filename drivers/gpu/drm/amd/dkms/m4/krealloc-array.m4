dnl #
dnl # v5.10-13-gf0dbd2bd1c22
dnl # mm: slab: provide krealloc_array()
dnl #
AC_DEFUN([AC_AMDGPU_KREALLOC_ARRAY], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/gfp.h>
			#include <linux/slab.h>
		], [
			void *p = krealloc_array(NULL, 0, 0, GFP_KERNEL);
			(void)p;
		], [
			AC_DEFINE(HAVE_KREALLOC_ARRAY, 1,
				[krealloc_array() is available])
		])
	])
])

dnl #
dnl #v6.11-rc6-3-g590b9d576cae mm: kvmalloc: align kvrealloc() with krealloc()
dnl #v6.9-rc4-85-g7bd230a26648 mm/slab: enable slab allocation tagging for kmalloc and friends
dnl #v5.15-11-g8587ca6f3415  mm: move kvmalloc-related functions to slab.h
dnl #v5.14-rc4-23-gde2860f46362  mm: Add kvrealloc()
dnl #
AC_DEFUN([AC_AMDGPU_KVREALLOC], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/gfp.h>
			#include <linux/slab.h>
			#include <linux/mm.h>
		], [
			void *p = NULL;
			p = kvrealloc(NULL, 0, GFP_KERNEL);
		], [
			AC_DEFINE(HAVE_KVREALLOC_3ARG, 1, [kvrealloc() has 3 arguments])
		],[
			AC_KERNEL_TRY_COMPILE([
				#include <linux/gfp.h>
				#include <linux/slab.h>
				#include <linux/mm.h>
			], [
				void *p = NULL;
				p = kvrealloc(NULL, 0, 0, GFP_KERNEL);
			], [
				AC_DEFINE(HAVE_KVREALLOC_4ARG, 1, [kvrealloc() has 4 arguments])
			],[
				AC_DEFINE(HAVE_NO_KVREALLOC, 1, [kvrealloc() isn't available])
			])
		])
	])
])

