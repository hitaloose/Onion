#!/bin/sh
# Builds Onion for the RG35XX and assembles the card layout in cache/rg35xx/sd.
# Needs the rootfs and SDK from platform/rg35xx/buildroot/build.sh.
# Usage (from the repo root): platform/rg35xx/dist.sh
#
# The Miyoo build copies prebuilt Miyoo binaries from static/ (MainUI, SDL,
# audioserver, jq, 7z, ...). They are glibc armv7ve builds that would shadow
# the rootfs tools, so every ELF file from static/ is left out here.
set -e

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
PLATFORM_DIR="$ROOT_DIR/platform/rg35xx"
CACHE_DIR="$ROOT_DIR/cache/rg35xx"
BIN_DIR="$CACHE_DIR/bin"
OUT="$CACHE_DIR/sd"
SYSDIR="$OUT/.tmp_update"
VERSION="$(make -s -C "$ROOT_DIR" version)-rg35xx"

PROGRAMS="bootScreen chargingState gameSwitcher keymon playActivity themeSwitcher
    tweaks packageManager sendkeys setState renameRom infoPanel prompt batmon
    easter detectKey pressMenu2Kill gameNameList sendUDP tree pippi
    randomGamePicker padmapd"
APPS="Tweaks ThemeSwitcher"

# Build
rm -rf "$BIN_DIR"
mkdir -p "$BIN_DIR"
"$PLATFORM_DIR/tools/make.sh" "
    set -e
    make -s -C include/SDL TARGET=../../lib/rg35xx/libSDL_rotozoom.so CC=\${CROSS_COMPILE}gcc
    for p in $(echo $PROGRAMS); do
        make -s -C src/\$p clean > /dev/null
        make -s -C src/\$p BUILD_DIR=/src/cache/rg35xx/bin VERSION=$VERSION
    done
    \${CROSS_COMPILE}gcc -Wall -O2 platform/rg35xx/tools/vtreset.c -o cache/rg35xx/bin/vtreset
    mkdir -p cache/rg35xx/apps/PlayActivity
    make -s -C src/playActivityUI clean > /dev/null
    make -s -C src/playActivityUI BUILD_DIR=/src/cache/rg35xx/apps/PlayActivity VERSION=$VERSION
"

# Layout
rm -rf "$OUT"
mkdir -p "$OUT"
rsync -a --exclude .gitkeep "$ROOT_DIR/static/build/" "$OUT/"
rsync -a --exclude .gitkeep "$ROOT_DIR/static/configs/" "$OUT/"
for app in $APPS; do
    rsync -a "$ROOT_DIR/static/packages/App/$app/" "$OUT/"
done
rsync -a "$ROOT_DIR/static/packages/App/Activity Tracker/" "$OUT/"

# Drop the Miyoo binaries and the Miyoo libraries (SDL, audio, MainUI hooks)
find "$OUT" -type f -exec sh -c 'head -c 4 "$1" | grep -q "^.ELF" && rm -f "$1"' _ {} \;
rm -rf "$OUT/miyoo/lib" "$OUT/8188fu.ko"

for res in "$ROOT_DIR"/src/*/res; do
    case "$res" in */installUI/res) continue ;; esac
    cp -R "$res/." "$SYSDIR/res/"
done
for script in "$ROOT_DIR"/src/packageManager/script "$ROOT_DIR"/src/themeSwitcher/script; do
    cp -R "$script/." "$SYSDIR/script/"
done

mkdir -p "$SYSDIR/bin" "$SYSDIR/lib" "$SYSDIR/onionVersion"
cp "$BIN_DIR"/* "$SYSDIR/bin/"
rm -f "$SYSDIR/bin/libgamename.so"
cp "$ROOT_DIR/lib/rg35xx/libSDL_rotozoom.so" "$SYSDIR/lib/"
cp "$CACHE_DIR/apps/PlayActivity/playActivityUI" "$OUT/App/PlayActivity/"
cp "$CACHE_DIR/rootfs.img" "$SYSDIR/rootfs.img"
printf "v%s" "$VERSION" > "$SYSDIR/onionVersion/version.txt"

# Temporary launcher until src/mainUI exists
cp "$PLATFORM_DIR/tools/mainui-stub.sh" "$OUT/miyoo/app/MainUI"
chmod +x "$OUT/miyoo/app/MainUI"

du -sh "$OUT"
