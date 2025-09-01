/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_MINMAX_H
#define _KCL_MINMAX_H

#include <linux/minmax.h>

#ifndef umin
#define umin(x, y)	\
	min((x) + 0u + 0ul + 0ull, (y) + 0u + 0ul + 0ull)
#endif

#ifndef MIN
#define __kcl_cmp_op_min <

#define __kcl_cmp(op, x, y)	((x) __kcl_cmp_op_##op (y) ? (x) : (y))

#define MIN(a, b) __kcl_cmp(min, a, b)
#endif

#endif /* _KCL_MINMAX_H */
