/*
 * Created: Fri Jan 19 10:48:35 2001 by faith@acm.org
 *
 * Copyright 2001 VA Linux Systems, Inc., Sunnyvale, California.
 * All Rights Reserved.
 *
 * Author Rickard E. (Rik) Faith <faith@valinux.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <kcl/kcl_drm_drv.h>

#ifndef HAVE_DRM_DEV_WEDGED_EVENT
/*
 * Available recovery methods for wedged device. To be sent along with device
 * wedged uevent.
 */
static const char *drm_get_wedge_recovery(unsigned int opt)
{
        switch (BIT(opt)) {
        case DRM_WEDGE_RECOVERY_NONE:
                return "none";
        case DRM_WEDGE_RECOVERY_REBIND:
                return "rebind";
        case DRM_WEDGE_RECOVERY_BUS_RESET:
                return "bus-reset";
        default:
                return NULL;
        }
}

/**
 * drm_dev_wedged_event - generate a device wedged uevent
 * @dev: DRM device
 * @method: method(s) to be used for recovery
 *
 * This generates a device wedged uevent for the DRM device specified by @dev.
 * Recovery @method\(s) of choice will be sent in the uevent environment as
 * ``WEDGED=<method1>[,..,<methodN>]`` in order of less to more side-effects.
 * If caller is unsure about recovery or @method is unknown (0),
 * ``WEDGED=unknown`` will be sent instead.
 *
 * Refer to "Device Wedging" chapter in Documentation/gpu/drm-uapi.rst for more
 * details.
 *
 * Returns: 0 on success, negative error code otherwise.
 */
int drm_dev_wedged_event(struct drm_device *dev, unsigned long method)
{
        const char *recovery = NULL;
        unsigned int len, opt;
        /* Event string length up to 28+ characters with available methods */
        char event_string[32];
        char *envp[] = { event_string, NULL };

        len = scnprintf(event_string, sizeof(event_string), "%s", "WEDGED=");

        for_each_set_bit(opt, &method, BITS_PER_TYPE(method)) {
                recovery = drm_get_wedge_recovery(opt);
                if (drm_WARN_ONCE(dev, !recovery, "invalid recovery method %u\n", opt))
                        break;

                len += scnprintf(event_string + len, sizeof(event_string), "%s,", recovery);
        }
        if (recovery)
                /* Get rid of trailing comma */
                event_string[len - 1] = '\0';
        else
                /* Caller is unsure about recovery, do the best we can at this point. */
                snprintf(event_string, sizeof(event_string), "%s", "WEDGED=unknown");

        drm_info(dev, "device wedged, %s\n", method == DRM_WEDGE_RECOVERY_NONE ?
                 "but recovered through reset" : "needs recovery");

        return kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE, envp);
}
EXPORT_SYMBOL(drm_dev_wedged_event);

#endif /* HAVE_DRM_DEV_WEDGED_EVENT */
