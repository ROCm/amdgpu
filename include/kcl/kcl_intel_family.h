/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_INTEL_FAMILY_H
#define AMDKCL_INTEL_FAMILY_H

#ifdef CONFIG_X86

#include <asm/intel-family.h>
#include <kcl/kcl_cpu_device_id.h>
/* Copied froma asm/intel-family.h*/
#ifndef INTEL_FAM6_ROCKETLAKE
#define INTEL_FAM6_ROCKETLAKE       0xA7
#endif

#ifndef INTEL_FAM6_ALDERLAKE
#define INTEL_FAM6_ALDERLAKE		0x97	/* Golden Cove / Gracemont */
#endif

#ifndef IFM
#define IFM(_fam, _model)	VFM_MAKE(X86_VENDOR_INTEL, _fam, _model)
#endif

#ifndef INTEL_ALDERLAKE
#define INTEL_ALDERLAKE			IFM(6, 0x97) /* Golden Cove / Gracemont */
#define INTEL_ALDERLAKE_L		IFM(6, 0x9A) /* Golden Cove / Gracemont */
#endif

#ifndef INTEL_ROCKETLAKE
#define INTEL_RAPTORLAKE		IFM(6, 0xB7) /* Raptor Cove / Enhanced Gracemont */
#define INTEL_RAPTORLAKE_P		IFM(6, 0xBA)
#define INTEL_RAPTORLAKE_S		IFM(6, 0xBF)
#endif

#endif /* CONFIG_X86 */
#endif
