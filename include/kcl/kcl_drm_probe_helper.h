// SPDX-License-Identifier: GPL-2.0 OR MIT

#ifndef __KCL_DRM_PROBE_HELPER_H__
#define __KCL_DRM_PROBE_HELPER_H__

#ifndef HAVE_DRM_CRTC_HELPER_MODE_VALID_FIXED
#include <drm/drm_probe_helper.h>
#include <drm/drm_modes.h>
#include <drm/drm_crtc.h>

enum drm_mode_status kcl_drm_crtc_helper_mode_valid_fixed(struct drm_crtc *crtc,
						      const struct drm_display_mode *mode,
						      const struct drm_display_mode *fixed_mode);

#define drm_crtc_helper_mode_valid_fixed kcl_drm_crtc_helper_mode_valid_fixed
#endif
#endif
