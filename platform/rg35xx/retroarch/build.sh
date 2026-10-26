#!/bin/sh
# Builds RetroArch for the RG35XX with Onion's patches (network commands used
# by GameSwitcher and keymon). The Miyoo drivers from RetroArch-patch are not
# used: video is the SDL 1.2 "sdl" driver on fbcon, or "gl" (OpenGL ES 2 on
# the PowerVR GPU, mali_fbdev context patched for its FLIP window system);
# audio is ALSA; input is the SDL joystick.
# Patches in platform/rg35xx/retroarch/patches apply on top of Onion's.
# Usage (from the repo root): platform/rg35xx/retroarch/build.sh [clean]
# Output: cache/rg35xx/retroarch/retroarch
set -e

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
PATCH_DIR="$ROOT_DIR/third-party/RetroArch-patch"

if [ ! -f "$PATCH_DIR/submodules/RetroArch/Makefile" ]; then
    git -C "$ROOT_DIR" submodule update --init third-party/RetroArch-patch
    git -C "$PATCH_DIR" submodule update --init --depth 1
fi

mkdir -p "$ROOT_DIR/cache/rg35xx/retroarch"
[ "$1" = "clean" ] && CLEAN=1 || CLEAN=0

"$ROOT_DIR/platform/rg35xx/tools/make.sh" '
    set -e
    BUILD=/work/retroarch
    [ '$CLEAN' = 1 ] && rm -rf $BUILD
    if [ ! -f $BUILD/.is_patched ]; then
        rm -rf $BUILD && mkdir -p $BUILD
        cp -r third-party/RetroArch-patch/submodules/RetroArch/. $BUILD/
        find $BUILD -type f \( -name "*.c" -o -name "*.h" \) -exec sed -i "s/\r$//" {} +
        for p in third-party/RetroArch-patch/patches/*.patch platform/rg35xx/retroarch/patches/*.patch; do
            patch -s -d $BUILD -p1 < $p
        done
        touch $BUILD/.is_patched
    fi
    cd $BUILD
    export PKG_CONF_PATH=pkg-config
    export PKG_CONFIG_PATH=$SYSROOT/usr/lib/pkgconfig
    export PKG_CONFIG_SYSROOT_DIR=$SYSROOT
    if [ ! -f config.mk ]; then
        CROSS_COMPILE=arm-buildroot-linux-gnueabihf- \
        CFLAGS="-marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard -O2 -DRG35XX" \
        ./configure --host=arm-buildroot-linux-gnueabihf --prefix=/usr \
            --enable-sdl --disable-sdl2 --enable-alsa --disable-oss \
            --disable-pulse --disable-jack --disable-x11 --disable-wayland \
            --disable-kms --enable-egl --disable-opengl --disable-opengl1 \
            --enable-opengles --disable-vulkan --disable-qt --disable-ffmpeg \
            --disable-materialui --disable-xmb --disable-ozone --enable-rgui \
            --enable-neon --enable-floathard --disable-udev --disable-dbus \
            --disable-discord --disable-ssl --disable-cheevos \
            --enable-networking --enable-command --enable-7zip --enable-zlib \
            --enable-threads --disable-videocore --enable-mali_fbdev
    fi
    make -j$(nproc) > /work/retroarch-build.log 2>&1 || { tail -40 /work/retroarch-build.log; exit 1; }
    ${CROSS_COMPILE}strip -o /src/cache/rg35xx/retroarch/retroarch retroarch
    file /src/cache/rg35xx/retroarch/retroarch
'
