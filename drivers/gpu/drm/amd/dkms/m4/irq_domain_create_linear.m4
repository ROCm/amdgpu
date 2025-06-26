dnl #
dnl # commit v4.3-rc5-47-g1bf4ddc46c5d
dnl # irqdomain: Introduce irq_domain_create_{linear, tree}
dnl #
AC_DEFUN([AC_AMDGPU_IRQ_DOMAIN_CREATE_LINEAR], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/irqdomain.h>
		], [
			irq_domain_create_linear(NULL, 0, NULL, NULL);
		],[
			AC_DEFINE(HAVE_IRQ_DOMAIN_CREATE_LINEAR, 1,
				[irq_domain_create_linear() is available])
		])
	])
])
