/* SPDX-License-Identifier: GPL-2.0+ */
#ifndef _KCL_KUNIT_H
#define _KCL_KUNIT_H

#include <kunit/visibility.h>

/*
 * Upstream keys these off CONFIG_KUNIT, but amdgpu guards its own KUnit
 * plumbing with CONFIG_DRM_AMD_DC_KUNIT_TEST. A CONFIG_KUNIT=y kernel then
 * exports symbols amdgpu keeps static and modpost fails.
 */
#if !IS_ENABLED(CONFIG_DRM_AMD_DC_KUNIT_TEST) || !defined(HAVE_KUNIT_VISIBILITY_H)
#undef VISIBLE_IF_KUNIT
#define VISIBLE_IF_KUNIT static
#undef EXPORT_SYMBOL_IF_KUNIT
#define EXPORT_SYMBOL_IF_KUNIT(symbol)
#endif

#endif
