/* SPDX-License-Identifier: GPL-2.0 */
#include <drm/drm_drv.h>
#include "amdgpu.h"

#ifdef AMDKCL_DEVM_DRM_DEV_ALLOC
/* Copied from v5.7-rc1-343-gb0b5849e0cc0 drivers/gpu/drm/drm_drv.c and modified for KCL */
void *__devm_drm_dev_alloc(struct device *parent, struct drm_driver *driver,
			   size_t size, size_t offset)
{
	void *container;
	struct drm_device *drm;
	int ret;

	container = kzalloc(size, GFP_KERNEL);
	if (!container)
		return ERR_PTR(-ENOMEM);

	drm = container + offset;
	ret = drm_dev_init(drm, driver, parent);
	if (ret) {
		drm_dev_put(drm);
		return ERR_PTR(ret);
	}
#ifdef HAVE_DRM_DRM_MANAGED_H
	drmm_add_final_kfree(drm, container);
#endif
	drm->dev_private = container;
	return container;
}

void amdkcl_drm_dev_release(struct drm_device *ddev)
{
	drm_dev_put(ddev);
}

#endif
