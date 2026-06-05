dnl #
dnl # v6.x removed signal_on_any parameter from dma_fence_array_create
dnl # Old: dma_fence_array_create(num_fences, fences, context, seqno, signal_on_any)
dnl # New: dma_fence_array_create(num_fences, fences, context, seqno)
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_ARRAY_CREATE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/dma-fence-array.h>
		], [
			dma_fence_array_create(0, NULL, 0, 0, false);
		], [dma_fence_array_create], [drivers/dma-buf/dma-fence-array.c], [
			AC_DEFINE(HAVE_DMA_FENCE_ARRAY_CREATE_5ARGS, 1,
				[dma_fence_array_create() has 5 args])
		])
	])
])
