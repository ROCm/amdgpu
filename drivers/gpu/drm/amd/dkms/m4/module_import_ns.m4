dnl #
dnl # v6.13-rc1-2-gcdd30ebb1b9f
dnl # module: Convert symbol namespace to string literal
dnl #
AC_DEFUN([AC_AMDGPU_MODULE_IMPORT_NS], [
        AC_KERNEL_DO_BACKGROUND([
                header_file_src=$LINUX/include/linux/module.h
                AS_IF([test -f "$header_file_src"], [
                        AS_IF([grep -qE '^#define MODULE_IMPORT_NS\(ns\)\s+MODULE_INFO\(import_ns, ns\)$' $header_file_src ], [
                        AC_DEFINE(HAVE_MODULE_IMPORT_NS_NEED_A_STRING, 1,
						[MODULE_IMPORT_NS() wants a string arguments])
                        ])
                ])
        ])
])
