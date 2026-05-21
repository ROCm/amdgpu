dnl #
dnl # v6.12-rc6-3-g1aec4bc15a3c
dnl # list_lru: remove the spinlock argument from the walk callback
dnl #
dnl # list_lru_walk_cb was changed from 4 params to 3 params:
dnl #   Old: (struct list_head *, struct list_lru_one *, spinlock_t *, void *)
dnl #   New: (struct list_head *, struct list_lru_one *, void *)
dnl #
AC_DEFUN([AC_AMDGPU_3ARGS_LIST_LRU_WALK_CB], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/atomic.h>
			#include <linux/list_lru.h>
			static enum lru_status test_cb(struct list_head *item,
						       struct list_lru_one *list,
						       void *cb_arg)
			{
				return LRU_SKIP;
			}
		], [
			list_lru_walk_cb cb = test_cb;
			(void)cb;
		], [
			AC_DEFINE(HAVE_3ARGS_LIST_LRU_WALK_CB, 1,
				[list_lru_walk_cb takes 3 args (no spinlock)])
		])
	])
])
