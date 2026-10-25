#!/bin/sh
# Builds the Onion RG35XX rootfs image and SDK inside the rg35xx-toolchain image.
# Usage (from the repo root): platform/rg35xx/buildroot/build.sh
# Output: cache/rg35xx/rootfs.img (copy it to .tmp_update/rootfs.img on the card)
#         cache/rg35xx/arm-buildroot-linux-gnueabihf_sdk-buildroot.tar.gz
# The Buildroot tree lives in the onion-rg35xx-br Docker volume, because Buildroot
# needs a case-sensitive filesystem; the build log is /work/build.log in it.
set -e
BR_VERSION=2025.02.19
VOLUME=onion-rg35xx-br
PLATFORM_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$PLATFORM_DIR/../../cache/rg35xx"

docker build -q -t rg35xx-toolchain "$PLATFORM_DIR/toolchain" > /dev/null
docker volume create $VOLUME > /dev/null
mkdir -p "$OUT_DIR"

docker run --rm \
    -v $VOLUME:/work \
    -v "$PLATFORM_DIR/buildroot":/external:ro \
    -v "$OUT_DIR":/out \
    rg35xx-toolchain sh -c "
set -e
[ -d buildroot-$BR_VERSION ] || curl -sL https://buildroot.org/downloads/buildroot-$BR_VERSION.tar.xz | tar -xJ
cd buildroot-$BR_VERSION
make -s BR2_EXTERNAL=/external rg35xx_onion_defconfig
make -j\$(nproc) > /work/build.log 2>&1 || { tail -60 /work/build.log; exit 1; }
make sdk >> /work/build.log 2>&1 || { tail -60 /work/build.log; exit 1; }
cp output/images/rootfs.ext4 /out/rootfs.img
cp output/images/*_sdk-buildroot.tar.gz /out/
"
ls -l "$OUT_DIR"
