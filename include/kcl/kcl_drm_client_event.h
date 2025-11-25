/* SPDX-License-Identifier: MIT */
#ifndef KCL_KCL_DRM_CLIENT_EVENT_H
#define KCL_KCL_DRM_CLIENT_EVENT_H

#include <drm/drm_fb_helper.h>
#include <drm/drm_client_event.h>

#ifndef HAVE_DRM_CLIENT_DEV_RESUME
void _kcl_drm_client_dev_resume(struct drm_device *dev);
void _kcl_drm_client_dev_suspend(struct drm_device *dev);
#define drm_client_dev_resume    _kcl_drm_client_dev_resume
#define drm_client_dev_suspend   _kcl_drm_client_dev_suspend
#endif

#endif
