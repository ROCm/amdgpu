/* SPDX-License-Identifier: GPL-2.0 */
/*
 * KUnit API providing hooks for non-test code to interact with tests.
 *
 * Copyright (C) 2020, Google LLC.
 * Author: Uriel Guajardo <urielguajardo@google.com>
 */

#ifndef _KCL_KUNIT_TEST_BUG_H
#define _KCL_KUNIT_TEST_BUG_H

#include <linux/stddef.h>
#include <kunit/test-bug.h>

#ifndef kunit_fail_current_test
#define kunit_fail_current_test(fmt, ...) do {} while (0)
#endif
#endif
