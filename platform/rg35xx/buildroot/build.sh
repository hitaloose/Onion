#!/bin/sh
# Builds the Onion RG35XX rootfs image and SDK.
# Usage: platform/rg35xx/buildroot/build.sh [buildroot-dir]
# Output: <buildroot-dir>/output/images/rootfs.ext4 (copy it to .tmp_update/rootfs.img)
#         <buildroot-dir>/output/images/arm-buildroot-linux-gnueabihf_sdk-buildroot.tar.gz
set -e
BR_VERSION=2025.02.7
EXTERNAL="$(cd "$(dirname "$0")" && pwd)"
BR_DIR="${1:-$EXTERNAL/../../../cache/buildroot-$BR_VERSION}"

if [ ! -d "$BR_DIR" ]; then
    mkdir -p "$(dirname "$BR_DIR")"
    curl -L "https://buildroot.org/downloads/buildroot-$BR_VERSION.tar.xz" | tar -xJ -C "$(dirname "$BR_DIR")"
fi

make -C "$BR_DIR" BR2_EXTERNAL="$EXTERNAL" rg35xx_onion_defconfig
make -C "$BR_DIR"
make -C "$BR_DIR" sdk
