/* SPDX-License-Identifier: GPL-2.0 */
#ifndef AMDKCL_RATELIMIT_H
#define AMDKCL_RATELIMIT_H

#include <linux/ratelimit.h>

#ifndef HAVE_RATELIMIT_STATE_RESET_INTERVAL

static inline int ratelimit_state_reset_miss(struct ratelimit_state *rs)
{
	int ret = rs->missed;

	rs->missed = 0;
	return ret;
}

static inline void ratelimit_state_reset_interval(struct ratelimit_state *rs, int interval_init)
{
	unsigned long flags;

	raw_spin_lock_irqsave(&rs->lock, flags);
	rs->interval = interval_init;
	rs->begin = 0;
	rs->printed = 0;
	ratelimit_state_reset_miss(rs);
	raw_spin_unlock_irqrestore(&rs->lock, flags);
}
#endif

#endif
