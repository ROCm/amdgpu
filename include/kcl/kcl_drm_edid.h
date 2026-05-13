#ifndef AMDKCL_DRM_EDID_H
#define AMDKCL_DRM_EDID_H

#include <drm/drm_edid.h>

#ifndef drm_edid_encode_panel_id
#define drm_edid_encode_panel_id(vend_chr_0, vend_chr_1, vend_chr_2, product_id) \
	((((u32)(vend_chr_0) - '@') & 0x1f) << 26 | \
	 (((u32)(vend_chr_1) - '@') & 0x1f) << 21 | \
	 (((u32)(vend_chr_2) - '@') & 0x1f) << 16 | \
	 ((product_id) & 0xffff))
#endif /* drm_edid_encode_panel_id */

#ifndef DRM_EDID_RANGE_OFFSET_MIN_VFREQ
#define DRM_EDID_RANGE_OFFSET_MIN_VFREQ (1 << 0) /* 1.4 */
#define DRM_EDID_RANGE_OFFSET_MAX_VFREQ (1 << 1) /* 1.4 */
#define DRM_EDID_RANGE_OFFSET_MIN_HFREQ (1 << 2) /* 1.4 */
#define DRM_EDID_RANGE_OFFSET_MAX_HFREQ (1 << 3) /* 1.4 */
#endif

#ifndef DRM_EDID_FEATURE_CONTINUOUS_FREQ
#define DRM_EDID_FEATURE_CONTINUOUS_FREQ  (1 << 0) /* 1.4 */
#endif


/*	commit v5.18-rc5-1046-ge4ccf9a777d3
	drm/edid: add struct drm_edid container	*/
#if !defined(HAVE_DRM_EDID_MALLOC) || !defined(HAVE_DRM_EDID_RAW) || !defined(HAVE_DRM_EDID_VALID) \
	|| !defined(HAVE_DRM_EDID_IS_DIGITAL)
struct drm_edid {
	/* Size allocated for edid */
	size_t size;
	struct edid *edid;
};
#endif

#ifndef HAVE_DRM_EDID_MALLOC
const struct drm_edid *_kcl_drm_edid_alloc(const void *edid, size_t size);
void _kcl_drm_edid_free(const struct drm_edid *drm_edid);
#define  drm_edid_alloc _kcl_drm_edid_alloc
#define  drm_edid_free _kcl_drm_edid_free
#endif

#ifndef HAVE_DRM_EDID_RAW
const struct edid *_kcl_drm_edid_raw(const struct drm_edid *drm_edid);
#define  drm_edid_raw _kcl_drm_edid_raw
#endif

#ifndef HAVE_DRM_EDID_VALID
static inline bool _kcl_drm_edid_valid(const struct drm_edid *drm_edid)
{
	if (!drm_edid)
		return false;

	return drm_edid_is_valid(drm_edid->edid);
}
#define  drm_edid_valid _kcl_drm_edid_valid
#endif

/*	commit v6.3-rc2-329-gf1e4c916f97f
	drm/edid: add drm_edid_dup()	*/
#ifndef HAVE_DRM_EDID_DUP
static inline const struct drm_edid *_kcl_drm_edid_dup(const struct drm_edid *drm_edid)
{
	if (!drm_edid)
		return NULL;
	return drm_edid_alloc(drm_edid_raw(drm_edid), drm_edid->size);
}
#define drm_edid_dup _kcl_drm_edid_dup
#endif

/*	commit v6.3-rc2-336-gce6526ab2364
	drm/edid: add drm_edid_read_ddc()	*/
#ifndef HAVE_DRM_EDID_READ_DDC
struct drm_connector;
struct i2c_adapter;
const struct drm_edid *_kcl_drm_edid_read_ddc(struct drm_connector *connector,
					       struct i2c_adapter *adapter);
#define drm_edid_read_ddc _kcl_drm_edid_read_ddc
#endif

/*	commit v5.20-rc1-474-g5ceb7058b5fe
	drm/edid: add drm_edid_connector_update()	*/
#ifndef HAVE_DRM_EDID_CONNECTOR_UPDATE
struct drm_connector;
int _kcl_drm_edid_connector_update(struct drm_connector *connector,
				    const struct drm_edid *drm_edid);
#define drm_edid_connector_update _kcl_drm_edid_connector_update
#endif

/*	commit v5.20-rc1-479-gedd1f90a8403
	drm/edid: add drm_edid_connector_add_modes()	*/
#ifndef HAVE_DRM_EDID_CONNECTOR_ADD_MODES
struct drm_connector;
int _kcl_drm_edid_connector_add_modes(struct drm_connector *connector);
#define drm_edid_connector_add_modes _kcl_drm_edid_connector_add_modes
#endif

/*	commit v6.3-rc2-327-g02364dd57e33
	drm/edid: add drm_edid_is_digital()	*/
#ifndef HAVE_DRM_EDID_IS_DIGITAL
static inline bool _kcl_drm_edid_is_digital(const struct drm_edid *drm_edid)
{
	const struct edid *edid;

	if (!drm_edid)
		return false;
	edid = drm_edid_raw(drm_edid);
	return edid && (edid->input & DRM_EDID_INPUT_DIGITAL);
}
#define drm_edid_is_digital _kcl_drm_edid_is_digital
#endif

#endif
