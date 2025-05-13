/* SPDX-License-Identifier: MIT */
#ifndef _KCL_DRM_DP_HELPER_BACKPORT_H_
#define _KCL_DRM_DP_HELPER_BACKPORT_H_

#include <kcl/kcl_drm_dp_helper.h>

#if !defined(HAVE_DRM_DP_CEC_REGISTER_CONNECTOR_PP)
#define drm_dp_cec_register_connector _kcl_drm_dp_cec_register_connector
#endif

#if !defined(HAVE_DRM_DP_READ_DPCD_CAPS)
int _kcl_drm_dp_read_dpcd_caps(struct drm_dp_aux *aux,
			  u8 dpcd[DP_RECEIVER_CAP_SIZE]);
static inline int drm_dp_read_dpcd_caps(struct drm_dp_aux *aux,
			  u8 dpcd[DP_RECEIVER_CAP_SIZE])
{
    return _kcl_drm_dp_read_dpcd_caps(aux, dpcd);
}
#endif
#endif
