dnl #
dnl # v6.0-rc2-1-g825cf206ed51
dnl # statx: add direct I/O alignment information
dnl #
AC_DEFUN([AC_AMDGPU_STATX_DIOALIGN], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/stat.h>
			#include <linux/fs.h>
		],[
			struct kstat st;
			st.dio_offset_align = 0;
			st.dio_mem_align = 0;
			(void)STATX_DIOALIGN;
		],[
			AC_DEFINE(HAVE_STATX_DIOALIGN, 1,
				[STATX_DIOALIGN and kstat dio_offset_align/dio_mem_align are available])
		])
	])
])
