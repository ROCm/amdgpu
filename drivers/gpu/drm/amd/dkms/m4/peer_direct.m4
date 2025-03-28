dnl #
dnl #
dnl # PEER DIRECT support
dnl #
dnl # Note that some kernels will have rdma/peer_mem.h and general P2P
dnl # support included. We should check for that case first. If so, we
dnl # set p2p support to 1.
dnl # If the system uses the ofa_kernel, which is present outside of the
dnl # base kernel, then check if it exists and has a p2p symbol present.
dnl # If so, we set p2p support to 1, but also require setting the path
dnl # to the ofa_kernel, which is checked in Kbuild.
dnl # This way we can support configs that have p2p integrated into the
dnl # base kernel, or configs using the ofa_kernel
AC_DEFUN([AC_AMDGPU_KFD_PEERDIRECT_SUPPORT], [
    AC_KERNEL_DO_BACKGROUND([
        AC_KERNEL_TRY_COMPILE([
            #include <rdma/peer_mem.h>
        ], [
            struct peer_memory_client client;
            ib_register_peer_memory_client(&client, NULL);
        ], [
            AC_DEFINE(HAVE_KFD_PEERDIRECT_SUPPORT, 1, [HAVE_KFD_PEERDIRECT_SUPPORT is available])
        ], [
            AS_IF([ grep -qw ib_register_peer_memory_client /usr/src/ofa_kernel/x86_64/${KERNELVER}/Module.symvers ], [
            AC_DEFINE(HAVE_KFD_PEERDIRECT_SUPPORT, 1, [HAVE_KFD_PEERDIRECT_SUPPORT is available])
            AC_DEFINE(HAVE_KFD_PEERDIRECT_SUPPORT_NEED_OFAPATH, 1, [HAVE_KFD_PEERDIRECT_SUPPORT is available and needs OFA path])
            ])
        ])
    ])
])

