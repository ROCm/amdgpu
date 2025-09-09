# SPDX-License-Identifier: MIT
#
# Copyright 2025 Advanced Micro Devices, Inc.
# Makefile for bounding box component.
# Floating point required due to nature of bounding box values

ifdef CONFIG_ARCH_HAS_KERNEL_FPU_SUPPORT
soc_and_ip_translator_ccflags := $(CC_FLAGS_FPU)
soc_and_ip_translator_rcflags := $(CC_FLAGS_NO_FPU)
else
ifdef CONFIG_X86
soc_and_ip_translator_ccflags-$(CONFIG_CC_IS_GCC) := -mhard-float
soc_and_ip_translator_ccflags := $(soc_and_ip_translator_ccflags-y) -msse
endif

ifdef CONFIG_PPC64
soc_and_ip_translator_ccflags := -mhard-float -maltivec
endif

ifdef CONFIG_ARM64
soc_and_ip_translator_rcflags := -mgeneral-regs-only
endif

ifdef CONFIG_LOONGARCH
soc_and_ip_translator_ccflags := -mfpu=64
soc_and_ip_translator_rcflags := -msoft-float
endif

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
soc_and_ip_translator_ccflags += -mpreferred-stack-boundary=4
else
soc_and_ip_translator_ccflags += -msse2
endif
endif

endif #CONFIG_ARCH_HAS_KERNEL_FPU_SUPPORT

CFLAGS_$(AMDDALPATH)/dc/soc_and_ip_translator/dcn401/dcn401_soc_and_ip_translator.o := $(soc_and_ip_translator_ccflags)

CFLAGS_REMOVE_$(AMDDALPATH)/dc/soc_and_ip_translator/dcn401/dcn401_soc_and_ip_translator.o := $(soc_and_ip_translator_rcflags)

soc_and_ip_translator := soc_and_ip_translator.o
soc_and_ip_translator += dcn401/dcn401_soc_and_ip_translator.o

AMD_DAL_soc_and_ip_translator := $(addprefix $(AMDDALPATH)/dc/soc_and_ip_translator/, $(soc_and_ip_translator))

AMD_DISPLAY_FILES += $(AMD_DAL_soc_and_ip_translator)
