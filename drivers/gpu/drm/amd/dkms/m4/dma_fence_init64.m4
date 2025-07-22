dnl #
dnl # commit v6.15-rc5-1889-gbf33a0003d9e
dnl # dma-fence: Use a flag for 64-bit seqnos
dnl #
AC_DEFUN([AC_AMDGPU_DMA_FENCE_INIT64], [
     AC_KERNEL_DO_BACKGROUND([
            AC_KERNEL_TRY_COMPILE([
                #include <linux/dma-fence.h>
            ], [
                dma_fence_init64(NULL, NULL, NULL, 0, 0);
            ], [
                AC_DEFINE(HAVE_DMA_FENCE_INIT64, 1,
                    [dma_fence_init64() is available])
            ])
    ])
])

