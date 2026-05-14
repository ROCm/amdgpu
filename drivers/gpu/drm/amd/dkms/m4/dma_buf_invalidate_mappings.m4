dnl #
dnl # v6.19-rc1-326-gef246da8e63c4
dnl # dma-buf: Rename .move_notify() callback to a clearer identifier
dnl #
AC_DEFUN([AC_AMDGPU_DMA_BUF_INVALIDATE_MAPPINGS], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <linux/dma-buf.h>
                ],[
                        struct dma_buf_attach_ops *ops = NULL;
                        ops->invalidate_mappings = NULL;
                ],[
                        AC_DEFINE(HAVE_DMA_BUF_ATTACH_OPS_INVALIDATE_MAPPINGS, 1,
                                [dma_buf_attach_ops.invalidate_mappings is available])
                ])
        ])
])
