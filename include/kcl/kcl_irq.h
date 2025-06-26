/* SPDX-License-Identifier: GPL-2.0-only */

#ifndef AMDKCL_IRQ_H
#define AMDKCL_IRQ_H

#include <linux/irqdomain.h>

#ifndef HAVE_IRQ_DOMAIN_CREATE_LINEAR
#define irq_domain_create_linear irq_domain_add_linear
#endif /* HAVE_IRQ_DOMAIN_CREATE_LINEAR */

#endif
