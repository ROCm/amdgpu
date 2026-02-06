dnl #
dnl # v6.8-rc3-9-g39714fd73c6b
dnl # PCI: Make pci_dev_is_disconnected() helper public for other drivers
dnl #
AC_DEFUN([AC_AMDGPU_PCI_DEV_IS_DISCONNECTED], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/pci.h>
		], [
			pci_dev_is_disconnected(NULL);
		], [
			AC_DEFINE(HAVE_PCI_DEV_IS_DISCONNECTED, 1, [pci_dev_is_disconnected is available])
		])
	])
])
