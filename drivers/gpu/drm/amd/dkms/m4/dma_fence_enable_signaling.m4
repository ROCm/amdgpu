dnl #
dnl # dma-buf: export dma_fence_enable_signaling()
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_ENABLE_SIGNALING], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/dma-fence.h>
		], [
			dma_fence_enable_signaling(NULL);
		], [
			AC_DEFINE(HAVE_DMA_FENCE_ENABLE_SIGNALING, 1,
				[dma_fence_enable_signaling() is available])
		])
	])
])
