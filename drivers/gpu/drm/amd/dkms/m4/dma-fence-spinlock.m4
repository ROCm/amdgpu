dnl #
dnl # v7.0-rc1-139-g1f32f310a13c9
dnl # dma-buf: inline spinlock for fence protection
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_SPINLOCK], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/dma-fence.h>
		], [
			spinlock_t *lock;
			lock = dma_fence_spinlock(NULL);
		], [
			AC_DEFINE(HAVE_DMA_FENCE_SPINLOCK, 1, [dma_fence_spinlock() is available])
		])
	])
])
