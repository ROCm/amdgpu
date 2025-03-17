
#ifndef AMDKCL_DRM_DRV_H
#define AMDKCL_DRM_DRV_H

#include <linux/bitops.h>
#include <drm/drm_device.h>
#include <drm/drm_print.h>
#include <drm/drm_file.h>
#include <kcl/kcl_drm_print.h>

#ifndef HAVE_DRM_DEV_WEDGED_EVENT
/*
 * Recovery methods for wedged device in order of less to more side-effects.
 * To be used with drm_dev_wedged_event() as recovery @method. Callers can
 * use any one, multiple (or'd) or none depending on their needs.
 */
#define DRM_WEDGE_RECOVERY_NONE         BIT(0)  /* optional telemetry collection */
#define DRM_WEDGE_RECOVERY_REBIND       BIT(1)  /* unbind + bind driver */
#define DRM_WEDGE_RECOVERY_BUS_RESET    BIT(2)  /* unbind + reset bus device + bind */

int drm_dev_wedged_event(struct drm_device *dev, unsigned long method);
#endif /* HAVE_DRM_DEV_WEDGED_EVENT */

#endif /* AMDKCL_DRM_DRV_H */

