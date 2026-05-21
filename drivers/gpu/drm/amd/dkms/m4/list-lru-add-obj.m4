dnl #
dnl # v6.8 0a97c01cd20bb
dnl # list_lru: allow explicit memcg and NUMA node selection
dnl #
dnl # list_lru_add_obj() was introduced as the 2-arg wrapper for the
dnl # new 4-arg list_lru_add(). Unlike list_lru_add(), list_lru_add_obj()
dnl # is always exported (including on RHEL 10.2 where list_lru_add is not).
dnl # We use HAVE_LIST_LRU_ADD_OBJ purely as a reliable compile-time signal
dnl # that the 4-arg list_lru_add() API exists.
dnl #
AC_DEFUN([AC_AMDGPU_LIST_LRU_ADD_OBJ], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/atomic.h>
			#include <linux/list_lru.h>
		], [
			bool ret;
			ret = list_lru_add_obj(NULL, NULL);
		], [
			AC_DEFINE(HAVE_LIST_LRU_ADD_OBJ, 1,
				[list_lru_add_obj() is available])
		])
	])
])
