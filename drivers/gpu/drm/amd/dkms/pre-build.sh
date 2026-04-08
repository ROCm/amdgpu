#!/bin/bash

KCL="amd/amdkcl"
INC="include"
SRC="amd/dkms"

KERNELVER=$1
DKMS_TREE=$2
MODULE_BUILD_DIR=$3
CC=$4
KERNELVER_BASE=${KERNELVER%%-*}

version_lt () {
    newest=$((echo "$KERNELVER_BASE"; echo "$1") | sort -V | tail -n1)
    [ "$KERNELVER_BASE" != "$newest" ]
}

version_ge () {
    newest=$((echo "$KERNELVER_BASE"; echo "$1") | sort -V | tail -n1)
    [ "$KERNELVER_BASE" = "$newest" ]
}

version_gt () {
    oldest=$((echo "$KERNELVER_BASE"; echo "$1") | sort -V | head -n1)
    [ "$KERNELVER_BASE" != "$oldest" ]
}

version_le () {
    oldest=$((echo "$KERNELVER_BASE"; echo "$1") | sort -V | head -n1)
    [ "$KERNELVER_BASE" = "$oldest" ]
}

if [ "$CC" == "gcc" ]; then
	# Enable gcc-toolset for kernels that are built with non-default compiler
	# perform this check only when permissions allow
	if [[ -d /opt/rh && `id -u` -eq 0 ]]; then
		for f in $(find /opt/rh -type f -a -name gcc); do
			[[ -f /boot/config-$KERNELVER ]] || continue
			config_gcc_version=$(. /boot/config-$KERNELVER && echo $CONFIG_GCC_VERSION)
			# CONFIG_GCC_VERSION is included in kernel v4.17-6936-ga4353898980c
			if [ ! "$config_gcc_version" ]; then
				config_gcc_version=$(strings /boot/vmlinuz-$KERNELVER | grep -F $KERNELVER | grep gcc | grep -oP 'gcc version \K[\d.]+')
			fi
			IFS='.' read -ra ver <<<$($f -dumpfullversion)
			gcc_version=$(printf "%d%02d%02d\n" ${ver[@]})
			if [[ "$config_gcc_version" = "$gcc_version" ]]; then
				. ${f%/*}/../../../enable
				break
			fi
		done
	fi

	gcc_version=$(($CC -dumpfullversion 2>/dev/null || $CC -dumpversion) | awk -F. '{printf "%d%02d%02d", $1, $2, $3}')
	kernel_version=$(uname -r | awk -F. '{printf "%d%02d", $1, $2}')
	# gcc 4.8.5 is too old for kernel >= 5.4, which will cause the compile failure.
	if [ "$gcc_version" -lt 40805 ] && [ "$kernel_version" -ge 0504 ]; then
	        echo "Error: The GCC is too old for this kernel, please update the GCC to higher than 9.3"
	        exit 1
	fi
fi

source $KCL/files

sed -i -e '/DEFINE_WD_CLASS(reservation_ww_class)/,/EXPORT_SYMBOL(reservation_ww_class)/d' \
       -e '/dma_resv_lockdep/,/subsys_initcall/d' \
       -e '1i\#ifdef HAVE_DMA_RESV_FENCES' \
       -e '$a\#endif' $KCL/dma-buf/dma-resv.c
sed -i -e '/extern struct ww_class reservation_ww_class/i #include <kcl/kcl_dma-resv.h>' \
       -e '/struct dma_resv {/, /}/d' \
       -e '/struct dma_resv_iter {/, /}/d' \
       -e '/enum dma_resv_usage {/, /}/d' $INC/linux/dma-resv.h

# add amd prefix to exported symbols
for file in $FILES; do
	awk -F'[()]' '/EXPORT_SYMBOL/ {
		print "#define "$2" amd"$2" //"$0
	}' $file | sort -u >>$INC/rename_symbol.h
done

# rename CONFIG_xxx to CONFIG_xxx_AMDKCL
# otherwise kernel config would override dkms package config
AMDGPU_CONFIG=$(find -name Kconfig -exec grep -h '^config' {} + | sed 's/ /_/' | tr 'a-z' 'A-Z')
TTM_CONFIG=$(awk '/CONFIG_DRM/{gsub(".*\\(CONFIG_DRM","CONFIG_DRM");gsub("\\).*","");print $0}' ttm/Makefile)
SCHED_CONFIG=$(awk '/CONFIG_DRM/{gsub(".*\\(CONFIG_DRM","CONFIG_DRM");gsub("\\).*","");print $0}' scheduler/Makefile)
for config in $AMDGPU_CONFIG $TTM_CONFIG $SCHED_CONFIG; do
	for file in $(grep -rl $config ./); do
		sed -i "s/\<$config\>/&_AMDKCL/" $file
	done
	sed -i "/${config}$/s/$/_AMDKCL/" amd/dkms/Kbuild
done

# The upstream fix is commit c6031b1dbbbf ("kbuild: make *.mod rule robust against
# too long argument error"), merged in v5.19. Kernels < 6.0 may still lack this fix.
# Fix "Argument list too long" in recursive make for large modules (700+ .o files).
# Makefile.build recurses into subdirectories via sub-makes that don't inherit
# overrides. Inject the include into amd/amdgpu/Makefile; cmd_mod_fix.mk itself
# checks whether the fix is needed and is a no-op on newer kernels.
if version_lt 6.0; then
	sed -i '1i\-include $(src)/../dkms/cmd_mod_fix.mk' amd/amdgpu/Makefile
fi

export KERNELVER
ln -s $DKMS_TREE $MODULE_BUILD_DIR
echo "PATH=$PATH" >$MODULE_BUILD_DIR/.env

# unset TMPDIR in this shell
# otherwise conflicting package libpam-tmpdir incorrectly generates config
unset TMPDIR

# Provide vmlinux for BTF generation when building out-of-tree (e.g. DKMS)
#
# BTF generation is OFF by default. To enable it, explicitly set:
#   export AMDGPU_BTF=1
#   sudo -E dkms install amdgpu
#
# When enabled, the following checks must all pass before the vmlinux
# symlink is created:
#   1. CONFIG_DEBUG_INFO_BTF_MODULES=y in kernel config
#   2. pahole (from dwarves package) is installed
#   3. resolve_btfids is available (if the kernel modfinal stage requires it)
#   4. /sys/kernel/btf/vmlinux is readable
#   5. vmlinux does not already exist in the build dir
#   6. build dir is writable
KVER="${KERNELVER}"
KDIR="/lib/modules/${KVER}/build"
AMDGPU_BTF="${AMDGPU_BTF:-0}"

if [ "${AMDGPU_BTF}" = "1" ]; then
	echo "BTF: enabled by AMDGPU_BTF=1, running safety checks..."

	KCONFIG=""
	if [ -f "/boot/config-${KVER}" ]; then
		KCONFIG="/boot/config-${KVER}"
	elif [ -f "${KDIR}/.config" ]; then
		KCONFIG="${KDIR}/.config"
	fi

	BTF_OK=true
	# Check 1: kernel must require BTF module generation
	if [ -z "${KCONFIG}" ] || ! grep -q 'CONFIG_DEBUG_INFO_BTF_MODULES=y' "${KCONFIG}"; then
		BTF_OK=false
		echo "BTF: skipping — CONFIG_DEBUG_INFO_BTF_MODULES=y not found in kernel config"
	fi
	# Check 2: pahole must be available
	if ! command -v pahole >/dev/null 2>&1; then
		BTF_OK=false
		echo "BTF: skipping — pahole not found"
	fi
	# Check 3: resolve_btfids must be available (if the kernel requires it)
	# On SUSE, kernel source and build dirs are split: Makefile.modfinal lives
	# in the source tree (/lib/modules/<ver>/source/), not the build obj dir
	# that KDIR points to.  Check both locations.
	MODFINAL=""
	if [ -f "${KDIR}/scripts/Makefile.modfinal" ]; then
		MODFINAL="${KDIR}/scripts/Makefile.modfinal"
	elif [ -d "/lib/modules/${KVER}/source" ] && \
	     [ -f "/lib/modules/${KVER}/source/scripts/Makefile.modfinal" ]; then
		MODFINAL="/lib/modules/${KVER}/source/scripts/Makefile.modfinal"
	fi
	if [ -n "${MODFINAL}" ] && \
	   grep -qi resolve_btfids "${MODFINAL}" && \
	   [ ! -x "${KDIR}/tools/bpf/resolve_btfids/resolve_btfids" ]; then
		BTF_OK=false
		echo "BTF: skipping — resolve_btfids required by ${MODFINAL}" \
		     "but not found at ${KDIR}/tools/bpf/resolve_btfids/resolve_btfids"
	fi

	if $BTF_OK && [ -r /sys/kernel/btf/vmlinux ] && [ ! -e "${KDIR}/vmlinux" ] && [ -w "${KDIR}" ]; then
		ln -sf /sys/kernel/btf/vmlinux "${KDIR}/vmlinux" && \
		touch "${MODULE_BUILD_DIR}/.btf_vmlinux_symlink_created" || true
	fi
fi

(cd $SRC && ./configure CC=${CC})

# rename CFLAGS_<path>target.o / CFLAGS_REMOVE_<path> to CFLAGS_target.o
# for kernel version < 5.3
if ! grep -q 'define HAVE_AMDKCL_FLAGS_TAKE_PATH' $SRC/config/config.h; then
	for file in $(grep -rl 'CFLAGS_' amd/display/); do
		sed -i 's|\(CFLAGS_[A-Z_]*\)$(AMDDALPATH)/.*/\(.*\.o\)|\1\2|' $file
	done
fi

# v6.11-rc6-3-g590b9d576cae mm: kvmalloc: align kvrealloc() with krealloc()
# Hardcoded modification for `drm_exec.c` to adapt `kvrealloc` function usage to four arguments.
# WARNING: Do NOT optimize this section. The change is essential to guarantee that if there are
# any semantic changes to 'size', it will trigger a DKMS installation failure.
# This ensures any changes to 'size' usage must be manually reviewed and addressed in this context.
if ! grep -q 'define HAVE_KVREALLOC_3ARG ' $SRC/config/config.h; then
	sed -i -e 's/kvrealloc(exec->objects, size + PAGE_SIZE, GFP_KERNEL)/kvrealloc(exec->objects, size, size + PAGE_SIZE, GFP_KERNEL)/' \
		drm_exec.c
fi
