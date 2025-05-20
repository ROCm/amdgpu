dnl #
dnl # v6.2-rc4-463-gf01b2b3ed873
dnl # shmem: add shmem_read_folio() and shmem_read_folio_gfp()
dnl #
AC_DEFUN([AC_AMDGPU_SHMEM_READ_FOLIO], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <linux/shmem_fs.h>
                ], [
                        shmem_read_folio(NULL, 0);
                ], [
                        AC_DEFINE(HAVE_SHMEM_READ_FOLIO, 1,
                                [shmem_read_folio() is available])
                ])
        ])
])