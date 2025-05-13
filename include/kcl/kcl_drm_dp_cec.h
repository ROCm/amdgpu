/* SPDX-License-Identifier: GPL-2.0 */
/*
 * DisplayPort CEC-Tunneling-over-AUX support
 *
 * Copyright 2018 Cisco Systems, Inc. and/or its affiliates. All rights reserved.
 */

#ifndef __KCL_KCL_DRM_DP_CEC_H__
#define __KCL_KCL_DRM_DP_CEC_H__

#include <drm/display/drm_dp_helper.h>

#if !defined(HAVE_DRM_DP_CEC_REGISTER_CONNECTOR_PP)
static inline void _kcl_drm_dp_cec_register_connector(struct drm_dp_aux *aux,
				   struct drm_connector *connector)
{
#ifdef CONFIG_DRM_DP_CEC
	if (WARN_ON(!aux->transfer))
		return;
#endif

	drm_dp_cec_register_connector(aux, connector->name, connector->dev->dev);
}
#endif


#endif
