dnl #
dnl # v6.14-1919-g554f79d8cf32
dnl # dma-buf: Add dma_fence_dedup_array
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_DEDUP_ARRAY], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/dma-fence-unwrap.h>
		], [
			dma_fence_dedup_array(NULL, 0);
		], [dma_fence_dedup_array], [drivers/dma-buf/dma-fence-unwrap.c],[
			AC_DEFINE(HAVE_DMA_FENCE_DEDUP_ARRAY, 1,
				[dma_fence_dedup_array() is available])
		])
	])
])