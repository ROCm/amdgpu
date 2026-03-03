#!/bin/bash

MODULE_BUILD_DIR=$1
KERNELVER=$2
BTF_STAMP_FILE="$MODULE_BUILD_DIR/.btf_vmlinux_symlink_created"
if [ -f "${BTF_STAMP_FILE}" ]; then
    unlink "/lib/modules/${KERNELVER}/build/vmlinux" 2>/dev/null || true
    rm -f "${BTF_STAMP_FILE}"
fi
rm -rf $MODULE_BUILD_DIR
