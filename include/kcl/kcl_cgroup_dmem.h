/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_CGROUP_DMEM_H
#define AMDKCL_CGROUP_DMEM_H

#include <linux/types.h>
#include <linux/compiler.h>

#ifndef HAVE_LINUX_CGROUP_DMEM_H
/*
 * Old kernels without <linux/cgroup_dmem.h>: provide the full new-style
 * (struct dmem_cgroup_init based) API as no-op stubs so callers keep building.
 * dmem cgroup accounting is simply disabled on such kernels.
 */
struct dmem_cgroup_pool_state;

/* Opaque definition of a cgroup region, used internally */
struct dmem_cgroup_region;

struct dmem_cgroup_ops {
	int (*reclaim)(struct dmem_cgroup_pool_state *pool,
		       u64 target_bytes, void *priv);
};

struct dmem_cgroup_init {
	u64 size;
	const struct dmem_cgroup_ops *ops;
	void *reclaim_priv;
};

static inline __printf(2, 3) struct dmem_cgroup_region *
kcl_dmem_cgroup_register_region(const struct dmem_cgroup_init *init,
			    const char *name_fmt, ...)
{
	return NULL;
}

static inline void kcl_dmem_cgroup_unregister_region(struct dmem_cgroup_region *region)
{ }

static inline int kcl_dmem_cgroup_try_charge(struct dmem_cgroup_region *region, u64 size,
					 struct dmem_cgroup_pool_state **ret_pool,
					 struct dmem_cgroup_pool_state **ret_limit_pool)
{
	*ret_pool = NULL;

	if (ret_limit_pool)
		*ret_limit_pool = NULL;

	return 0;
}

static inline void kcl_dmem_cgroup_uncharge(struct dmem_cgroup_pool_state *pool, u64 size)
{ }

static inline
bool kcl_dmem_cgroup_state_evict_valuable(struct dmem_cgroup_pool_state *limit_pool,
				      struct dmem_cgroup_pool_state *test_pool,
				      bool ignore_low, bool *ret_hit_low)
{
	return true;
}

static inline bool kcl_dmem_cgroup_below_min(struct dmem_cgroup_pool_state *root,
					 struct dmem_cgroup_pool_state *test)
{
	return false;
}

static inline bool kcl_dmem_cgroup_below_low(struct dmem_cgroup_pool_state *root,
					 struct dmem_cgroup_pool_state *test)
{
	return false;
}

static inline
struct dmem_cgroup_pool_state *kcl_dmem_cgroup_get_common_ancestor(struct dmem_cgroup_pool_state *a,
							       struct dmem_cgroup_pool_state *b)
{
	return NULL;
}

static inline void kcl_dmem_cgroup_pool_state_put(struct dmem_cgroup_pool_state *pool)
{ }

#define dmem_cgroup_register_region(...) \
	kcl_dmem_cgroup_register_region(__VA_ARGS__)
#define dmem_cgroup_unregister_region		kcl_dmem_cgroup_unregister_region
#define dmem_cgroup_try_charge			kcl_dmem_cgroup_try_charge
#define dmem_cgroup_uncharge			kcl_dmem_cgroup_uncharge
#define dmem_cgroup_state_evict_valuable	kcl_dmem_cgroup_state_evict_valuable
#define dmem_cgroup_below_min			kcl_dmem_cgroup_below_min
#define dmem_cgroup_below_low			kcl_dmem_cgroup_below_low
#define dmem_cgroup_get_common_ancestor		kcl_dmem_cgroup_get_common_ancestor
#define dmem_cgroup_pool_state_put		kcl_dmem_cgroup_pool_state_put

#elif !defined(HAVE_STRUCT_DMEM_CGROUP_INIT)
/*
 * Kernels that ship <linux/cgroup_dmem.h> but still use the older
 * dmem_cgroup_register_region(u64 size, const char *name_fmt, ...) prototype.
 * Provide the struct dmem_cgroup_init based helpers and redirect the register
 * call to the real (size based) function. The .ops/.reclaim_priv fields are
 * ignored on such kernels.
 *
 * Include the kernel header first so its real dmem_cgroup_register_region()
 * declaration is parsed before we define the redirecting macro below,
 * otherwise the macro would corrupt the kernel prototype.
 */
#include <linux/cgroup_dmem.h>
#include <linux/stdarg.h>
#include <linux/string.h>

struct dmem_cgroup_ops {
	int (*reclaim)(struct dmem_cgroup_pool_state *pool,
		       u64 target_bytes, void *priv);
};

struct dmem_cgroup_init {
	u64 size;
	const struct dmem_cgroup_ops *ops;
	void *reclaim_priv;
};

/*
 * The reclaim-aware helpers below were introduced together with
 * struct dmem_cgroup_init and are absent on kernels that still use the
 * size-based register prototype. Provide no-op stubs so callers keep building;
 * reclaim accounting is simply disabled on such kernels.
 */
static inline bool kcl_dmem_cgroup_below_min(struct dmem_cgroup_pool_state *root,
					 struct dmem_cgroup_pool_state *test)
{
	return false;
}

static inline bool kcl_dmem_cgroup_below_low(struct dmem_cgroup_pool_state *root,
					 struct dmem_cgroup_pool_state *test)
{
	return false;
}

static inline
struct dmem_cgroup_pool_state *kcl_dmem_cgroup_get_common_ancestor(struct dmem_cgroup_pool_state *a,
							       struct dmem_cgroup_pool_state *b)
{
	return NULL;
}

#define dmem_cgroup_below_min			kcl_dmem_cgroup_below_min
#define dmem_cgroup_below_low			kcl_dmem_cgroup_below_low
#define dmem_cgroup_get_common_ancestor		kcl_dmem_cgroup_get_common_ancestor

/*
 * Bridge the struct dmem_cgroup_init based register API to the real size-based
 * kernel function. A plain macro cannot be used here because the caller passes
 * a compound literal (&(struct dmem_cgroup_init){ ... }) whose braces contain
 * commas that the preprocessor would treat as macro argument separators.
 * Forward through a variadic wrapper instead: the region name is formatted
 * locally and the .ops/.reclaim_priv fields are ignored on such kernels.
 */
static inline __printf(2, 3) struct dmem_cgroup_region *
kcl_dmem_cgroup_register_region(const struct dmem_cgroup_init *init,
				const char *name_fmt, ...)
{
	char name[64];
	va_list args;

	va_start(args, name_fmt);
	vsnprintf(name, sizeof(name), name_fmt, args);
	va_end(args);

	return dmem_cgroup_register_region(init->size, "%s", name);
}

#define dmem_cgroup_register_region(...) \
	kcl_dmem_cgroup_register_region(__VA_ARGS__)

#endif /* HAVE_LINUX_CGROUP_DMEM_H / HAVE_STRUCT_DMEM_CGROUP_INIT */

#endif
