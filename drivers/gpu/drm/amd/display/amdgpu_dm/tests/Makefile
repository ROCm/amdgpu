# SPDX-License-Identifier: GPL-2.0
#
# Makefile for amdgpu_dm KUnit tests.

ccflags-y += -I$(src)/..
ccflags-y += -I$(src)/../..
ccflags-y += -I$(src)/../../include
ccflags-y += -I$(src)/../../modules/inc
ccflags-y += -I$(src)/../../dc
ccflags-y += -I$(src)/../../../amdgpu

obj-$(CONFIG_DRM_AMD_DC_KUNIT_TEST) += amdgpu_dm_crc_test.o
obj-$(CONFIG_DRM_AMD_DC_KUNIT_TEST) += amdgpu_dm_hdcp_test.o
obj-$(CONFIG_DRM_AMD_DC_KUNIT_TEST) += amdgpu_dm_color_test.o
obj-$(CONFIG_DRM_AMD_DC_KUNIT_TEST) += amdgpu_dm_colorop_test.o
