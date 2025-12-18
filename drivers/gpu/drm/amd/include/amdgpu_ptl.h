/*
 * Copyright 2025 Advanced Micro Devices, Inc.
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

#ifndef __AMDGPU_PTL_H__
#define __AMDGPU_PTL_H__

enum amdgpu_ptl_fmt {
	AMDGPU_PTL_FMT_I8      = 0,
	AMDGPU_PTL_FMT_F16     = 1,
	AMDGPU_PTL_FMT_BF16    = 2,
	AMDGPU_PTL_FMT_F32     = 3,
	AMDGPU_PTL_FMT_F64     = 4,
	AMDGPU_PTL_FMT_INVALID = 5,
};

enum amdgpu_ptl_hw_supported_state {
	AMDGPU_PTL_HW_UNINIT = 0,       /* Not yet initialized */
	AMDGPU_PTL_HW_SUPPORTED,        /* Initialized and supported */
	AMDGPU_PTL_HW_NOT_SUPPORTED,    /* Initialized and not supported */
};

enum psp_ptl_cmd {
	PSP_PTL_PERF_MON_QUERY = 0xA0000000,
	PSP_PTL_PERF_MON_SET = 0xA0000001,
};

enum psp_ptl_format_type
{
	GFX_FTYPE_I8          = 0x00000000,
	GFX_FTYPE_F16         = 0x00000001,
	GFX_FTYPE_BF16        = 0x00000002,
	GFX_FTYPE_F32         = 0x00000003,
	GFX_FTYPE_F64         = 0x00000004,
	GFX_FTYPE_INVALID     = 0xFFFFFFFF,
};

struct psp_ptl_perf_req {
	enum psp_ptl_cmd req;
	uint32_t ptl_state;
	uint32_t pref_format1;
	uint32_t pref_format2;
};

extern const struct attribute_group amdgpu_ptl_attr_group;
int amdgpu_ptl_sysfs_init(struct amdgpu_device *adev);
void amdgpu_ptl_sysfs_fini(struct amdgpu_device *adev);

#endif /* __AMDGPU_PTL_H__ */
