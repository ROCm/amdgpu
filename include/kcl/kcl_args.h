/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _KCL_KCL_ARGS_H
#define _KCL_KCL_ARGS_H

#include <linux/args.h>

#ifndef __COUNT_ARGS
#define __COUNT_ARGS(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _n, X...) _n
#endif

#ifndef COUNT_ARGS
#define COUNT_ARGS(X...) __COUNT_ARGS(, ##X, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#endif

/* Concatenate two parameters, but allow them to be expanded beforehand. */
#ifndef __CONCAT
#define __CONCAT(a, b) a ## b
#endif

#ifndef CONCATENATE
#define CONCATENATE(a, b) __CONCAT(a, b)
#endif

#endif	/* _KCL_KCL_ARGS_H */
