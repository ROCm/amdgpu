dnl #
dnl # v6.15-rc5-1889-gbf33a0003d9e
dnl # dma-fence: Add dma_fence_was_initialized()
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_WAS_INITIALIZED], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/dma-fence.h>
		], [
			dma_fence_was_initialized(NULL);
		], [
			AC_DEFINE(HAVE_DMA_FENCE_WAS_INITIALIZED, 1,
				[dma_fence_was_initialized() is available])
		])
	])
])
