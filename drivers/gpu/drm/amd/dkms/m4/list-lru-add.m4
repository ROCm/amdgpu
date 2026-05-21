dnl #
dnl # v6.8 0a97c01cd20bb
dnl # list_lru: allow explicit memcg and NUMA node selection
dnl #
dnl # list_lru_add() was changed from 2 args to 4 args:
dnl #   Old: list_lru_add(lru, item)
dnl #   New: list_lru_add(lru, item, nid, memcg)
dnl #
dnl # Note: Use TRY_COMPILE_SYMBOL to verify list_lru_add is exported.
dnl # RHEL 10.2 has the 4-arg signature in headers but does not export it.
dnl #
AC_DEFUN([AC_AMDGPU_4ARGS_LIST_LRU_ADD], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE_SYMBOL([
			#include <linux/atomic.h>
			#include <linux/list_lru.h>
		], [
			bool ret;
			ret = list_lru_add(NULL, NULL, 0, NULL);
		], [list_lru_add], [mm/list_lru.c], [
			AC_DEFINE(HAVE_4ARGS_LIST_LRU_ADD, 1,
				[list_lru_add() takes 4 args and is exported])
		])
	])
])
