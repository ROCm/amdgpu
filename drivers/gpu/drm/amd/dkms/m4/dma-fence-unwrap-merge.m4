dnl #
dnl # v5.19-rc2-285-g245a4a7b531c
dnl # dma-buf: generalize dma_fence unwrap & merging v3
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_UNWRAP_MERGE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/dma-fence-unwrap.h>
		], [
			__dma_fence_unwrap_merge(0, NULL, NULL);
		], [__dma_fence_unwrap_merge], [drivers/dma-buf/dma-fence-unwrap.c],[
			AC_DEFINE(HAVE_DMA_FENCE_UNWRAP_MERGE, 1,
				[dma_fence_unwrap_merge() is available])
		])
	])
])
