#!/bin/bash

local_config=$1
KERNELVER=$2
local_config_tmp=${local_config}.tmp
config_file="/lib/modules/$KERNELVER/build/include/config/auto.conf"

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
    grep -q "^$1=\[ym]" "${config_file}"
}

export_macro_mk CONFIG_HSA_AMD
export_macro_mk CONFIG_DRM_AMDGPU_CIK
export_macro_mk CONFIG_DRM_AMDGPU_SI
export_macro_mk CONFIG_DRM_AMDGPU_USERPTR
export_macro_mk CONFIG_DRM_AMD_DC

cat ${local_config_tmp}
mv ${local_config_tmp} ${local_config}