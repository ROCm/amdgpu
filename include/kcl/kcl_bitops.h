/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_BITOPS_BACKPORT_H
#define AMDKCL_BITOPS_BACKPORT_H

#include <linux/bitops.h>
/* Copied froma include/linux/bitops.h */
#ifndef BITS_PER_TYPE
#define BITS_PER_TYPE(type)     (sizeof(type) * BITS_PER_BYTE)
#endif

/*
 * v6.16-rc1-70-g19408200c094
 * bits: introduce fixed-type GENMASK_U*()
 */
#ifndef GENMASK_U8
#define GENMASK_U8(h, l)	((u8)GENMASK(h, l))
#endif
#ifndef GENMASK_U16
#define GENMASK_U16(h, l)	((u16)GENMASK(h, l))
#endif
#ifndef GENMASK_U32
#define GENMASK_U32(h, l)	((u32)GENMASK(h, l))
#endif
#ifndef GENMASK_U64
#define GENMASK_U64(h, l)	((u64)GENMASK_ULL(h, l))
#endif

#endif
