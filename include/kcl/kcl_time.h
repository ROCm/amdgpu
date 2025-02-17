/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_TIME_H
#define _KCL_TIME_H

#include <linux/cache.h>
#include <linux/math64.h>
#include <linux/time64.h>

#ifndef HAVE_TIME64_TO_TM
void time64_to_tm(time64_t totalsecs, int offset, struct tm *result);
#endif

#endif /* _KCL_TIME_H */
