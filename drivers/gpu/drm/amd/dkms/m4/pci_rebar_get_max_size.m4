dnl #
dnl # v6.18-rc1-19-g1c680f2acdbb
dnl # PCI: Add pci_rebar_get_max_size()
dnl #
AC_DEFUN([AC_AMDGPU_PCI_REBAR_GET_MAX_SIZE], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/pci.h>
		], [
			pci_rebar_get_max_size(NULL, 0);
		], [
			AC_DEFINE(HAVE_PCI_REBAR_GET_MAX_SIZE, 1,
				[pci_rebar_get_max_size() is available])
		])
	])
])