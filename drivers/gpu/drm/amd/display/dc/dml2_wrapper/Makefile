# SPDX-License-Identifier: MIT
#
# Copyright 2023 Advanced Micro Devices, Inc.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
# Authors: AMD
#
# Makefile for dml2_wrapper.

# Must be evaluated before any include, otherwise MAKEFILE_LIST no longer ends
# with this file.
THIS_MAKEFILE_PATH := $(abspath $(lastword $(MAKEFILE_LIST)))

ifdef CONFIG_ARCH_HAS_KERNEL_FPU_SUPPORT
dml2_wrapper_ccflags := $(CC_FLAGS_FPU)
dml2_wrapper_rcflags := $(CC_FLAGS_NO_FPU)
else
ifdef CONFIG_X86
dml2_wrapper_ccflags-$(CONFIG_CC_IS_GCC) := -mhard-float
dml2_wrapper_ccflags := $(dml2_wrapper_ccflags-y) -msse
endif

ifdef CONFIG_PPC64
dml2_wrapper_ccflags := -mhard-float -maltivec
endif

ifdef CONFIG_ARM64
dml2_wrapper_rcflags := -mgeneral-regs-only
endif

ifdef CONFIG_LOONGARCH
dml2_wrapper_ccflags := -mfpu=64
dml2_wrapper_rcflags := -msoft-float
endif

include  $(src)/../dkms/Makefile.compiler

ifdef CONFIG_CC_IS_GCC
ifneq ($(call gcc-min-version, 70100),y)
IS_OLD_GCC = 1
endif
endif

ifdef CONFIG_X86
ifdef IS_OLD_GCC
# Stack alignment mismatch, proceed with caution.
# GCC < 7.1 cannot compile code using `double` and -mpreferred-stack-boundary=3
# (8B stack alignment).
dml2_wrapper_ccflags += -mpreferred-stack-boundary=4
else
dml2_wrapper_ccflags += -msse2
endif
endif

endif #CONFIG_ARCH_HAS_KERNEL_FPU_SUPPORT

subdir-ccflags-y += -I$(FULL_AMD_DISPLAY_PATH)/dc/dml2_wrapper
subdir-ccflags-y += -I$(FULL_AMD_DISPLAY_PATH)/dc/dml2_wrapper/dml21_wrapper

# Add FPU flags to all dml2_wrapper files by default, remove NO_FPU flags.
# FPU flags step 1: Find all .c files in dal/dc/dml2_wrapper and it's subfolders
DML2_WRAPPER_ABS_PATH := $(patsubst %/,%,$(dir $(THIS_MAKEFILE_PATH)))

rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$(2)) $(filter $(subst *,%,$(2)),$d))
DML2_WRAPPER_C_FILES := $(call rwildcard,$(DML2_WRAPPER_ABS_PATH)/,*.c)

# FPU flags step 2: Convert to .o and make paths relative to $(AMDDALPATH)/dc/dml2_wrapper/
DML2_WRAPPER_RELATIVE_O_FILES := $(patsubst $(DML2_WRAPPER_ABS_PATH)/%,dc/dml2_wrapper/%,$(patsubst %.c,%.o,$(DML2_WRAPPER_C_FILES)))

# FPU flags step 3: Apply FPU flags to all .o files from dal/dc/dml2_wrapper and it's subfolders
$(foreach obj,$(DML2_WRAPPER_RELATIVE_O_FILES),$(eval CFLAGS_$(AMDDALPATH)/$(obj) := $(dml2_wrapper_ccflags)))
$(foreach obj,$(DML2_WRAPPER_RELATIVE_O_FILES),$(eval CFLAGS_REMOVE_$(AMDDALPATH)/$(obj) := $(dml2_wrapper_rcflags)))

# FPU flags step 4: Replace CFLAGS per file for files with additional flags beyond dml2_ccflags and dml2_rcflags
CFLAGS_$(AMDDALPATH)/dc/dml2_wrapper/dml2_wrapper.o := $(dml2_wrapper_rcflags)
CFLAGS_REMOVE_$(AMDDALPATH)/dc/dml2_wrapper/dml2_wrapper.o := $(dml2_wrapper_ccflags)
CFLAGS_$(AMDDALPATH)/dc/dml2_wrapper/dml21_wrapper/dml21_wrapper.o := $(dml2_wrapper_rcflags)
CFLAGS_REMOVE_$(AMDDALPATH)/dc/dml2_wrapper/dml21_wrapper/dml21_wrapper.o := $(dml2_wrapper_ccflags)

DML2_WRAPPER = dml2_wrapper_fpu.o dml2_wrapper.o \
		dml2_utils.o dml2_policy.o dml2_translation_helper.o dml2_dc_resource_mgmt.o dml2_mall_phantom.o

DML21_WRAPPER = dml21_wrapper/dml21_wrapper_fpu.o dml21_wrapper/dml21_wrapper.o \
		dml21_wrapper/dml21_utils.o dml21_wrapper/dml21_translation_helper.o

AMD_DAL_DML2_WRAPPER = $(addprefix $(AMDDALPATH)/dc/dml2_wrapper/,$(DML2_WRAPPER))
AMD_DAL_DML21_WRAPPER = $(addprefix $(AMDDALPATH)/dc/dml2_wrapper/,$(DML21_WRAPPER))

AMD_DISPLAY_FILES += $(AMD_DAL_DML2_WRAPPER)
AMD_DISPLAY_FILES += $(AMD_DAL_DML21_WRAPPER)
