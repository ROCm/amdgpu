/* SPDX-License-Identifier: MIT */
#ifndef _KCL_HEADER_LINUX_DEV_PRINTK_H
#define _KCL_HEADER_LINUX_DEV_PRINTK_H

#ifdef HAVE_LINUX_DEV_PRINTK_H
#include_next <linux/dev_printk.h>
#else
#include <linux/device.h>
#endif

#endif