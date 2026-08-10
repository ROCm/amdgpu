/* SPDX-License-Identifier: MIT */
#ifndef KCL_KCL_LINUX_UNITS_H
#define KCL_KCL_LINUX_UNITS_H

#include <linux/units.h>

#ifndef HZ_PER_MHZ
#define HZ_PER_MHZ              1000000UL
#endif

/*
 * Fill in power-unit constants that are missing on older kernels: either the
 * <linux/units.h> header does not exist at all (e.g. 5.4), or it exists but
 * predates these defines (e.g. RHEL 8.x 4.18). Values copied verbatim from
 * upstream include/linux/units.h.
 */
#ifndef MILLIDEGREE_PER_DEGREE
#define MILLIDEGREE_PER_DEGREE	1000
#endif
#ifndef MILLIWATT_PER_WATT
#define MILLIWATT_PER_WATT	1000UL
#endif
#ifndef MICROWATT_PER_MILLIWATT
#define MICROWATT_PER_MILLIWATT	1000UL
#endif
#ifndef MICROWATT_PER_WATT
#define MICROWATT_PER_WATT	1000000UL
#endif

#endif

