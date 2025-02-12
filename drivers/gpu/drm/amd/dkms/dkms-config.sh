#!/bin/bash

local_config=$1
KERNELVER=$2
CC=$3
local_config_tmp=${local_config}.tmp
config_file="/lib/modules/$KERNELVER/build/include/config/auto.conf"
kcl_config_file="amd/dkms/config/config.h"

rm -f ${local_config_tmp}

append_mk () {
    echo $1 >> ${local_config_tmp}
}

export_macro_mk  () {
cat <<_DKMS_CONFIG >> ${local_config_tmp}
export $1=y
subdir-ccflags-y += -D$1

_DKMS_CONFIG
}

get_config() {
    grep "^$1=" "${config_file}" | awk -F= '{print $2}'
}

is_enabled() {
    grep -q "^$1=" "${config_file}"
}

is_kcl_macro_defined() {
    grep -q "define $1" "${kcl_config_file}" && echo "y" || echo "n"
}

# Export OS variables
if [[ -f /etc/os-release ]]; then
    OS_NAME=$(grep -oP '(?<=^ID=).+' /etc/os-release | tr -d '"')
    OS_VERSION=$(grep -oP '(?<=^VERSION_ID=).+' /etc/os-release | tr -d '"')
else
    OS_NAME="unknown"
    OS_VERSION="0.0"
fi
append_mk "export OS_NAME=${OS_NAME}"
append_mk "export OS_VERSION=${OS_VERSION}"

VERSION_MAJOR=$(echo ${OS_VERSION}.0 | cut -d. -f1)
VERSION_MINOR=$(echo ${OS_VERSION}.0 | cut -d. -f2)
OS_VERSION_STR=${OS_VERSION//./_}
OS_NAME_STR=$(echo ${OS_NAME} | tr a-z A-Z)

# Generate macros related to the operating system to support compilation.
# The OS name is converted to uppercase and appended as a flag, e.g., -DOS_NAME_UBUNTU, -DOS_NAME_RHEL.
append_mk "subdir-ccflags-y += -DOS_NAME_${OS_NAME_STR}"

# Add OS specific compile flags
append_mk "subdir-ccflags-y += -DOS_VERSION_MAJOR=${VERSION_MAJOR}"
append_mk "subdir-ccflags-y += -DOS_VERSION_MINOR=${VERSION_MINOR}"

# Add additional OS specific configurations
case "${OS_NAME}" in
    "opensuse-leap"|"sled"|"sles")
        append_mk "subdir-ccflags-y += -DOS_NAME_SUSE_${OS_VERSION_STR}"
        ;;
    "ubuntu")
        OS_BUILD_NUM=$(echo "${KERNELVER}" | cut -d '-' -f 2)
        append_mk "subdir-ccflags-y += -DUBUNTU_BUILD_NUM=${OS_BUILD_NUM}"
        OS_OEM=$(echo "${KERNELVER}" | cut -d '-' -f 3)
        if [[ "${OS_OEM}" == "oem" ]]; then
            append_mk "subdir-ccflags-y += -DOS_NAME_UBUNTU_OEM"
        fi
        append_mk "subdir-ccflags-y += -DOS_NAME_UBUNTU_${OS_VERSION_STR}"
        ;;
    "rhel")
        append_mk "subdir-ccflags-y += -DOS_NAME_RHEL_${OS_VERSION_STR}"
        if [[ "${VERSION_MAJOR}" == "7" ]]; then
            append_mk "subdir-ccflags-y += -DOS_NAME_RHEL_7_X"
            append_mk "-include /usr/src/kernels/${KERNELVER}/include/drm/drm_backport.h"
        elif [[ "${VERSION_MAJOR}" == "8" ]]; then
            append_mk "subdir-ccflags-y += -DOS_NAME_RHEL_8_X"
        fi
        ;;
esac

if [[ "$(get_config CONFIG_PCI_P2PDMA)" == "y" ]]; then
    if [[ "$(get_config CONFIG_DMABUF_MOVENOTIFY)" == "y" ]]; then
        export_macro_mk CONFIG_HSA_AMD_P2P
    fi
fi

# Check for HMM mirror configuration
if [[ "$(is_kcl_macro_defined HAVE_AMDKCL_HMM_MIRROR_ENABLED)" == "y" ]]; then
    if is_enabled CONFIG_DEVICE_PRIVATE; then
        export_macro_mk CONFIG_HSA_AMD_SVM
    fi
fi

export_macro_mk CONFIG_HSA_AMD
export_macro_mk CONFIG_DRM_AMDGPU_CIK
export_macro_mk CONFIG_DRM_AMDGPU_SI
export_macro_mk CONFIG_DRM_AMDGPU_USERPTR
export_macro_mk CONFIG_DRM_AMD_DC

cat ${local_config_tmp}
mv ${local_config_tmp} ${local_config}
