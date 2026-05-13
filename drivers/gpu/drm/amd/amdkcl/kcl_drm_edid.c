// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 Intel Corporation.
 *
 * Authors:
 * Ramalingam C <ramalingam.c@intel.com>
 */
#include <kcl/kcl_drm_edid.h>
#include <linux/slab.h>
#include <drm/drm_connector.h>
#include <drm/drm_property.h>

#ifndef HAVE_DRM_EDID_MALLOC
static const struct drm_edid *__kcl_drm_edid_alloc(const void *edid, size_t size)
{
	struct drm_edid *drm_edid;

	if (!edid || !size || size < EDID_LENGTH)
		return NULL;

	drm_edid = kzalloc(sizeof(*drm_edid), GFP_KERNEL);
	if (drm_edid) {
		drm_edid->edid = edid;
		drm_edid->size = size;
	}

	return drm_edid;
}

const struct drm_edid *_kcl_drm_edid_alloc(const void *edid, size_t size)
{
	const struct drm_edid *drm_edid;

	if (!edid || !size || size < EDID_LENGTH)
		return NULL;

	edid = kmemdup(edid, size, GFP_KERNEL);
	if (!edid)
		return NULL;

	drm_edid = __kcl_drm_edid_alloc(edid, size);
	if (!drm_edid)
		kfree(edid);

	return drm_edid;
}
EXPORT_SYMBOL(_kcl_drm_edid_alloc);

void _kcl_drm_edid_free(const struct drm_edid *drm_edid)
{
	if (!drm_edid)
		return;

	kfree(drm_edid->edid);
	kfree(drm_edid);
}
EXPORT_SYMBOL(_kcl_drm_edid_free);

const struct drm_edid *kcl_drm_edid_dup(const struct drm_edid *drm_edid)
{
	if (!drm_edid)
		return NULL;
	return _kcl_drm_edid_alloc(drm_edid->edid, drm_edid->size);
}
EXPORT_SYMBOL(kcl_drm_edid_dup);

const struct drm_edid *kcl_drm_edid_read_ddc(struct drm_connector *connector,
 struct i2c_adapter *adapter)
{
	struct edid *edid;

	if (connector->force == DRM_FORCE_OFF)
		return NULL;

	if (connector->force == DRM_FORCE_ON && !connector->override_edid)
		edid = drm_get_edid(connector, adapter);
	else
		edid = drm_get_edid(connector, adapter);

	if (!edid)
		return NULL;

	return __kcl_drm_edid_alloc(edid, (edid->extensions + 1) * EDID_LENGTH);
}
EXPORT_SYMBOL(kcl_drm_edid_read_ddc);

#endif

#ifndef HAVE_DRM_EDID_RAW
static int edid_extension_block_count(const struct edid *edid)
{
	return edid->extensions;
}

static int edid_block_count(const struct edid *edid)
{
	return edid_extension_block_count(edid) + 1;
}

static int edid_size_by_blocks(int num_blocks)
{
	return num_blocks * EDID_LENGTH;
}

static int edid_size(const struct edid *edid)
{
	return edid_size_by_blocks(edid_block_count(edid));
}

const struct edid *_kcl_drm_edid_raw(const struct drm_edid *drm_edid)
{
	if (!drm_edid || !drm_edid->size)
		return NULL;

	/*
	 * Do not return pointers where relying on EDID extension count would
	 * lead to buffer overflow.
	 */
	if (WARN_ON(edid_size(drm_edid->edid) > drm_edid->size))
		return NULL;

	return drm_edid->edid;
}
EXPORT_SYMBOL(_kcl_drm_edid_raw);
#endif

#ifndef HAVE_DRM_EDID_READ_DDC
const struct drm_edid *_kcl_drm_edid_read_ddc(struct drm_connector *connector,
					       struct i2c_adapter *adapter)
{
	struct edid *edid;
	const struct drm_edid *drm_edid;

	edid = drm_get_edid(connector, adapter);
	if (!edid)
		return NULL;

	drm_edid = drm_edid_alloc(edid, (edid->extensions + 1) * EDID_LENGTH);
	kfree(edid);
	return drm_edid;
}
EXPORT_SYMBOL(_kcl_drm_edid_read_ddc);
#endif

#ifndef HAVE_DRM_EDID_CONNECTOR_UPDATE
int _kcl_drm_edid_connector_update(struct drm_connector *connector,
				    const struct drm_edid *drm_edid)
{
	return drm_connector_update_edid_property(connector,
		drm_edid ? (struct edid *)drm_edid_raw(drm_edid) : NULL);
}
EXPORT_SYMBOL(_kcl_drm_edid_connector_update);
#endif

#ifndef HAVE_DRM_EDID_CONNECTOR_ADD_MODES
int _kcl_drm_edid_connector_add_modes(struct drm_connector *connector)
{
	struct edid *edid = NULL;

	if (connector->edid_blob_ptr)
		edid = (struct edid *)connector->edid_blob_ptr->data;

	if (!edid)
		return 0;

	return drm_add_edid_modes(connector, edid);
}
EXPORT_SYMBOL(_kcl_drm_edid_connector_add_modes);
#endif
