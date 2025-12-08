/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _KCL_TIME_H
#define _KCL_TIME_H

#include <linux/cache.h>
#include <linux/math64.h>
#include <linux/time64.h>
#include <linux/timer.h>

#ifndef timer_container_of
#define timer_container_of(var, callback_timer, timer_fieldname)	\
	container_of(callback_timer, typeof(*var), timer_fieldname)
#endif

#ifndef HAVE_TIMER_DELETE
#define timer_delete del_timer
#define timer_delete_sync del_timer_sync
#endif


#endif /* _KCL_TIME_H */
