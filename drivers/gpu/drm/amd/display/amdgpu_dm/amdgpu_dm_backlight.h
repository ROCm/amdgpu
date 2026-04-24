/* SPDX-License-Identifier: MIT */
/*
 * Copyright 2026 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __AMDGPU_DM_BACKLIGHT_H__
#define __AMDGPU_DM_BACKLIGHT_H__

struct amdgpu_display_manager;
struct amdgpu_dm_connector;
struct drm_connector;
struct attribute_group;

void amdgpu_dm_update_backlight_caps(struct amdgpu_display_manager *dm,
				     int bl_idx);
void amdgpu_dm_backlight_set_level(struct amdgpu_display_manager *dm,
				   int bl_idx, u32 user_brightness);
void amdgpu_dm_register_backlight_device(struct amdgpu_dm_connector *aconnector);
void amdgpu_dm_setup_backlight_device(struct amdgpu_display_manager *dm,
			    struct amdgpu_dm_connector *aconnector);
void amdgpu_dm_update_connector_ext_caps(struct amdgpu_dm_connector *aconnector);
bool amdgpu_dm_should_create_sysfs(struct amdgpu_dm_connector *aconnector);

extern const struct attribute_group amdgpu_group;

#endif /* __AMDGPU_DM_BACKLIGHT_H__ */
