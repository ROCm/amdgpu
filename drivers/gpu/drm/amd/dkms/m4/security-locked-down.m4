dnl #
dnl # commit 000d388ed3bb ("lockdown: Lock down perf when in confidentiality mode")
dnl # v5.2-2-g9e47d31d6a57
dnl #
AC_DEFUN([AC_AMDGPU_SECURITY_LOCKED_DOWN], [
        AC_KERNEL_DO_BACKGROUND([
                AC_KERNEL_TRY_COMPILE([
                        #include <linux/security.h>
                ],[
                        int ret;
                        ret = security_locked_down(LOCKDOWN_PCI_ACCESS);
                ],[
                        AC_DEFINE(HAVE_SECURITY_LOCKED_DOWN, 1,
                                [security_locked_down(LOCKDOWN_PCI_ACCESS) is available])
                ])
        ])
])
