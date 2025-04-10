/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_CPU_DEVICE_ID_H
#define AMDKCL_CPU_DEVICE_ID_H

#ifdef CONFIG_X86

#include <asm/cpu_device_id.h>
/* Copied froma arch/x86/include/asm/cpu_device_id.h*/
#ifndef VFM_MODEL
#define VFM_MODEL_BIT	0
#define VFM_FAMILY_BIT	8
#define VFM_VENDOR_BIT	16

#define	VFM_MODEL_MASK	GENMASK(VFM_FAMILY_BIT - 1, VFM_MODEL_BIT)

#define VFM_MODEL(vfm)	(((vfm) & VFM_MODEL_MASK) >> VFM_MODEL_BIT)

#define	VFM_MAKE(_vendor, _family, _model) (	\
	((_model) << VFM_MODEL_BIT) |		\
	((_family) << VFM_FAMILY_BIT) |		\
	((_vendor) << VFM_VENDOR_BIT)		\
)

#endif

#endif /* CONFIG_X86 */
#endif
