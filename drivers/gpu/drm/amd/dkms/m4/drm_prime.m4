dnl #
dnl # commit v5.10-rc3-1140-gc67e62790f5c
dnl # drm/prime: split array import functions v4
dnl #
AC_DEFUN([AC_AMDGPU_DRM_PRIME_SG_TO_DMA_ADDR_ARRAY], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_prime.h>
		],[
			drm_prime_sg_to_dma_addr_array(NULL, NULL, 0);
		],[drm_prime_sg_to_dma_addr_array], [drivers/gpu/drm/drm_prime.c], [
			AC_DEFINE(HAVE_DRM_PRIME_SG_TO_DMA_ADDR_ARRAY, 1, 
				[drm_prime_sg_to_dma_addr_array() is available])
		])
	])
])

dnl #
dnl # commit v3.3-9296-g3248877ea179 
dnl # drm: base prime/dma-buf support (v5)
dnl #
dnl # commit v6.4-rc7-1904-g71a7974ac701
dnl # drm/prime: Unexport helpers for fd/handle conversion
dnl #
AC_DEFUN([AC_AMDGPU_DRM_GEM_PRIME_HANDLE_TO_FD], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE_SYMBOL([
                        #include <drm/drm_prime.h>
                ],[
                        drm_gem_prime_handle_to_fd(NULL, NULL, 0, 0, NULL);
                ],[drm_gem_prime_handle_to_fd], [drivers/gpu/drm/drm_prime.c], [
                        AC_DEFINE(HAVE_DRM_GEM_PRIME_HANDLE_TO_FD, 1,
                                [drm_gem_prime_handle_to_fd() is available])
                ])
        ])
])

dnl #
dnl # commit 6619ccf1bb1d
dnl # dma-buf: Use struct dma_buf_map in dma_buf_vmap() interfaces
dnl #
dnl # commit 20e76f1a7059
dnl # dma-buf: Use struct dma_buf_map in dma_buf_vunmap() interfaces
dnl #
dnl # commit c67e62790f5c
dnl # drm/prime: split array import functions v4
dnl #
AC_DEFUN([AC_AMDGPU_DRM_GEM_DMABUF_VMAP_HAS_IOSYS_MAP_ARG], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <drm/drm_prime.h>
		],[
			struct iosys_map map;
			drm_gem_dmabuf_vmap(NULL, &map);
		],[drm_gem_dmabuf_vmap], [drivers/gpu/drm/drm_prime.c], [
			AC_DEFINE(HAVE_DRM_GEM_DMABUF_VMAP_HAS_IOSYS_MAP_ARG, 1,
				[drm_gem_dmabuf_vmap() has struct iosys_map arg])
		])
	])
])
