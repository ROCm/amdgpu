#include <kcl/kcl_drm_client_event.h>

#ifndef HAVE_DRM_CLIENT_DEV_RESUME
void _kcl_drm_client_dev_resume(struct drm_device *dev)
{
	drm_fb_helper_set_suspend_unlocked(dev->fb_helper, true);
}
EXPORT_SYMBOL(_kcl_drm_client_dev_resume);

void _kcl_drm_client_dev_suspend(struct drm_device *dev)
{
	drm_fb_helper_set_suspend_unlocked(dev->fb_helper, false);
}
EXPORT_SYMBOL(_kcl_drm_client_dev_suspend);
#endif
