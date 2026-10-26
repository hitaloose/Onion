#!/bin/sh
# Builds the RG35XX kernel (uImage, device trees, modules) inside the
# rg35xx-kernel-toolchain image: rg35xx-cfw/s500_kernel plus patches/,
# drivers/, rg35xx.config and dts/ from this directory.
# Usage (from the repo root): platform/rg35xx/kernel/build.sh [make targets]
# Output: cache/rg35xx/kernel/{uImage,kernel.dtb,dtbs/<capacity>.dtb,modules/},
#         laid out like the misc partition
# The source tree lives in the rg35xx-kernel Docker volume (the kernel needs a
# case-sensitive filesystem); the build log is /work/build.log in it.
set -e
KERNEL_REPO=https://github.com/rg35xx-cfw/s500_kernel.git
KERNEL_BRANCH=rg35xx
DEFCONFIG=rg35xx_atm7039_defconfig
VOLUME=rg35xx-kernel
KERNEL_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="$KERNEL_DIR/../../../cache/rg35xx/kernel"
TARGETS="${*:-uImage modules}"

docker build -q --platform linux/amd64 -t rg35xx-kernel-toolchain "$KERNEL_DIR/toolchain" > /dev/null
docker volume create $VOLUME > /dev/null
mkdir -p "$OUT_DIR"

docker run --rm --platform linux/amd64 \
    -v $VOLUME:/work \
    -v "$KERNEL_DIR":/kernel:ro \
    -v "$OUT_DIR":/out \
    rg35xx-kernel-toolchain sh -c "
set -e
[ -d s500_kernel ] || git clone -q -b $KERNEL_BRANCH $KERNEL_REPO s500_kernel
cd s500_kernel
# Apply each patch once: skip the ones that already reverse-apply cleanly
for p in /kernel/patches/*.patch; do
    git apply -R --check \$p 2>/dev/null || git apply \$p
done
cp /kernel/drivers/rg35xx-gamepad.c drivers/input/joystick/
export ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
make -s $DEFCONFIG
scripts/kconfig/merge_config.sh -m .config /kernel/rg35xx.config > /dev/null
make -s olddefconfig
make -j\$(nproc) LOADADDR=0x00008000 $TARGETS > /work/build.log 2>&1 || { grep -B2 -A8 -m5 'error' /work/build.log | tail -60; exit 1; }
rm -rf /out/*
cp arch/arm/boot/uImage /out/
mkdir -p /out/dtbs /out/modules
# Padding leaves U-Boot room to add bootargs and memory to the tree, as the
# stock kernel.dtb does
dtc() { scripts/dtc/dtc -q -p 4096 -I dts -O dtb -i /kernel/dts -o \$2 /kernel/dts/\$1; }
dtc rg35xx.dts /out/kernel.dtb
cp /out/kernel.dtb /out/dtbs/2600mAh.dtb
dtc rg35xx-2100mAh.dts /out/dtbs/2100mAh.dtb
dtc rg35xx-2800mAh.dts /out/dtbs/2800mAh.dtb
find . -name '*.ko' -not -path './android/*' -exec cp {} /out/modules/ \;
"
ls -l "$OUT_DIR" "$OUT_DIR/dtbs"
