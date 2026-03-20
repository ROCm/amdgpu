dnl #
dnl # v5.8-rc2-471-g5fbff813a4a3
dnl # dma-fence: basic lockdep annotations
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_SIGNALLING], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/dma-fence.h>
		],[
			dma_fence_begin_signalling();
		],[
			AC_DEFINE(HAVE_DMA_FENCE_SIGNALLING, 1,
				[test whether dma_fence_begin_signalling are available])
		])
	])
])
