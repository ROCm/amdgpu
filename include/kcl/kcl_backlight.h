/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Backlight Lowlevel Control Abstraction
 *
 * Copyright (C) 2003,2004 Hewlett-Packard Company
 *
 */
#ifndef AMDKCL_BACKLIGHT_H
#define AMDKCL_BACKLIGHT_H

#include <linux/backlight.h>

#ifndef BACKLIGHT_POWER_ON
#define BACKLIGHT_POWER_ON              (0)
#define BACKLIGHT_POWER_OFF             (4)
#define BACKLIGHT_POWER_REDUCED         (1) // deprecated; don't use in new code
#endif

#endif
