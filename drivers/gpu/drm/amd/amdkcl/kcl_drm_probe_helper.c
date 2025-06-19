#include <kcl/kcl_drm_probe_helper.h>

#ifndef HAVE_DRM_CRTC_HELPER_MODE_VALID_FIXED
/**
 * drm_crtc_helper_mode_valid_fixed - Validates a display mode
 * @crtc: the crtc
 * @mode: the mode to validate
 * @fixed_mode: the display hardware's mode
 *
 * Returns:
 * MODE_OK on success, or another mode-status code otherwise.
 */
enum drm_mode_status kcl_drm_crtc_helper_mode_valid_fixed(struct drm_crtc *crtc,
						      const struct drm_display_mode *mode,
						      const struct drm_display_mode *fixed_mode)
{
	if (mode->hdisplay != fixed_mode->hdisplay && mode->vdisplay != fixed_mode->vdisplay)
		return MODE_ONE_SIZE;
	else if (mode->hdisplay != fixed_mode->hdisplay)
		return MODE_ONE_WIDTH;
	else if (mode->vdisplay != fixed_mode->vdisplay)
		return MODE_ONE_HEIGHT;

	return MODE_OK;
}
EXPORT_SYMBOL(kcl_drm_crtc_helper_mode_valid_fixed);
#endif
