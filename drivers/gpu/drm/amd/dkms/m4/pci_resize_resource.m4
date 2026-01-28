dnl #
dnl # v6.18-rc1-12-g9f71938cd77f
dnl # PCI: Move Resizable BAR code to rebar.c
dnl #
AC_DEFUN([AC_AMDGPU_PCI_RESIZE_RESOURCE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/pci.h>
		], [
			int ret = 0;
			ret = pci_resize_resource(NULL, 0, 0, 0);
		], [
			AC_DEFINE(HAVE_PCI_RESIZE_RESOURCE_ALIGN, 1,
				[pci_resize_resource() has align parameter])
		])
	])
])