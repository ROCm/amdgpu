dnl #
dnl # Oracle UEK preserves KABI by wrapping struct list_lru's node pointer with
dnl #   UEK_KABI_REPLACE(struct list_lru_node *node, struct list_lru_ext *ext)
dnl # In a normal (non-__GENKSYMS__) module build this expands to a union whose
dnl # accessible member is ->ext, and the per-node array lives at
dnl # lru->ext->node[] instead of the upstream lru->node[].
dnl #
dnl # Used by the KCL list_lru_add reimplementation to reach the per-node
dnl # sublist on UEK kernels.
dnl #
AC_DEFUN([AC_AMDGPU_LIST_LRU_EXT], [
	AC_KERNEL_DO_BACKGROUND([
		AC_KERNEL_TRY_COMPILE([
			#include <linux/atomic.h>
			#include <linux/list_lru.h>
		], [
			struct list_lru *lru = NULL;
			struct list_lru_node *nlru = &lru->ext->node[0];
			(void)nlru;
		], [
			AC_DEFINE(HAVE_LIST_LRU_EXT, 1,
				[struct list_lru wraps node[] inside ->ext (UEK KABI)])
		])
	])
])
