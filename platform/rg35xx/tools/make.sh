#!/bin/sh
# Runs a command in the rg35xx-toolchain image with the Buildroot SDK set up
# for PLATFORM=rg35xx. Needs platform/rg35xx/buildroot/build.sh to have run.
# Usage (from the repo root): platform/rg35xx/tools/make.sh make -C src/infoPanel
set -e
ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
SDK_TARBALL=cache/rg35xx/arm-buildroot-linux-gnueabihf_sdk-buildroot.tar.gz

docker run --rm \
    -v onion-rg35xx-br:/work \
    -v "$ROOT_DIR":/src \
    -w /src \
    -e PLATFORM=rg35xx \
    -e CROSS_COMPILE=/work/sdk/bin/arm-buildroot-linux-gnueabihf- \
    -e SYSROOT=/work/sdk/arm-buildroot-linux-gnueabihf/sysroot \
    rg35xx-toolchain sh -c "
set -e
if [ ! -x /work/sdk/bin/arm-buildroot-linux-gnueabihf-gcc ]; then
    mkdir -p /work/sdk
    tar -xzf $SDK_TARBALL -C /work/sdk --strip-components=1
    /work/sdk/relocate-sdk.sh > /dev/null
fi
export PATH=/work/sdk/bin:\$PATH
$*
"
