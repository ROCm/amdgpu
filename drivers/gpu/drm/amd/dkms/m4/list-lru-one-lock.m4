dnl #
dnl # v6.13 fb56fdf8b9a2f ("mm/list_lru: split the lock to per-cgroup scope")
dnl # moved the per-sublist spinlock from struct list_lru_node into
dnl # struct list_lru_one, and made struct list_lru_node.nr_items atomic_long_t.
dnl #
dnl #   Old layout: lock/nr_items live in struct list_lru_node (nlru->lock,
dnl #               nlru->nr_items is a plain long protected by that lock).
dnl #   New layout: lock lives in struct list_lru_one (l->lock) and
dnl #               struct list_lru_node.nr_items is atomic_long_t.
dnl #
dnl # Used by the KCL list_lru_add reimplementation to lock/count correctly.
dnl #
AC_DEFUN([AC_AMDGPU_LIST_LRU_ONE_LOCK], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/atomic.h>
			#include <linux/spinlock.h>
			#include <linux/list_lru.h>
		], [
			struct list_lru_one *l = NULL;
			spinlock_t *lock = &l->lock;
			(void)lock;
		], [
			AC_DEFINE(HAVE_LIST_LRU_ONE_LOCK, 1,
				[struct list_lru_one has a lock member])
		])
	])
])
