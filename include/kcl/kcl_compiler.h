/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_KCL_COMPILER_H
#define _KCL_KCL_COMPILER_H

#include <linux/compiler.h>
#include <kcl/kcl_compiler_types.h>

#ifndef __must_be_cstr
#define __must_be_cstr(p)	BUILD_BUG_ON_ZERO(__annotated(p, nonstring))
#endif

#endif /* _KCL_KCL_COMPILER_H */