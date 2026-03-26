dnl #
dnl # v6.18-rc2-1390-gc891b99d25dd
dnl # dma-buf/dma-fence: Add dma_fence_check_and_signal()
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_CHECK_AND_SIGNAL], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/dma-fence.h>
		], [
			dma_fence_check_and_signal(NULL);
		], [dma_fence_check_and_signal], [drivers/dma-buf/dma-fence.c],[
			AC_DEFINE(HAVE_DMA_FENCE_CHECK_AND_SIGNAL, 1,
				[dma_fence_check_and_signal() is available])
		])
	])
])