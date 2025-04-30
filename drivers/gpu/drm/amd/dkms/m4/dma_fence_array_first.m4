dnl #
dnl # v5.17-rc6-1495-gcaaf2ae712b7
dnl # dma-buf: Add dma_fence_array_for_each (v2)
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_ARRAY_FIRST], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/dma-fence-array.h>
		], [
                dma_fence_array_first(NULL);
		], [dma_fence_array_first], [drivers/dma-buf/dma-fence-array.c],[
			AC_DEFINE(HAVE_DMA_FENCE_ARRAY_FIRST, 1,
				[dma_fence_array_first() is available])
		])
	])
])
