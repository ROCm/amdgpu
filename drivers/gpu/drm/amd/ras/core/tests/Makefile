# SPDX-License-Identifier: GPL-2.0 OR MIT
#
# Makefile for the AMDGPU unified RAS core KUnit tests.
#
# Copyright 2026 Advanced Micro Devices, Inc.

# Same include paths as the amdgpu module objects that use ras.h
ccflags-y += -I$(src)/..
ccflags-y += -I$(src)/../../ras_mgr
ccflags-y += -I$(src)/../../../include
ccflags-y += -I$(src)/../../../include/asic_reg
ccflags-y += -I$(src)/../../../amdgpu
ccflags-y += -I$(src)/../../../pm/inc
ccflags-y += -I$(src)/../../../acp/include
ccflags-y += -I$(src)/../../../display
ccflags-y += -I$(src)/../../../display/include
ccflags-y += -I$(src)/../../../display/modules/inc
ccflags-y += -I$(src)/../../../display/dc
ccflags-y += -I$(src)/../../../display/amdgpu_dm
ccflags-y += -I$(src)/../../../amdkfd

obj-$(CONFIG_DRM_AMDGPU_RAS_KUNIT_TEST) += aca_v5_0_test.o
