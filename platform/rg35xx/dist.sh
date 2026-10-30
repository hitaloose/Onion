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
    randomGamePicker padmapd mainUI"
APPS="Tweaks ThemeSwitcher"
# Emulator packages whose cores platform/rg35xx/retroarch/cores.sh builds
EMU_PACKAGES="
Nintendo - Game Boy (Gambatte)
Nintendo - Game Boy Color (Gambatte)
Nintendo - GBA (gpSP)
Nintendo - NES (FCEUmm)
Nintendo - Famicom Disk Syst. (FCEUmm)
Nintendo - SNES (Beetle Supafaust)
Sega - Genesis (PicoDrive)
Sega - Master System (PicoDrive)
Sega - Game Gear (PicoDrive)
Sega - CD (PicoDrive)
Sega - 32X (PicoDrive)
Sony - PlayStation (PCSX ReARMed)
NEC - TurboGrafx-16 (Beetle PCE FAST)
NEC - TurboGrafx CD (Beetle PCE FAST)
"

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
    \${CROSS_COMPILE}gcc -Wall -O2 -static platform/rg35xx/tools/loopro.c -o cache/rg35xx/bin/loopro
    mkdir -p cache/rg35xx/apps/PlayActivity
    make -s -C src/playActivityUI clean > /dev/null
    make -s -C src/playActivityUI BUILD_DIR=/src/cache/rg35xx/apps/PlayActivity VERSION=$VERSION
"

# Layout
rm -rf "$OUT"
mkdir -p "$OUT"
rsync -a --exclude .gitkeep "$ROOT_DIR/static/build/" "$OUT/"
rsync -a --exclude .gitkeep "$ROOT_DIR/static/configs/" "$OUT/"
# RG35XX defaults on top (cpuclock.txt per core, in this CPU's MHz)
rsync -a "$PLATFORM_DIR/configs/" "$OUT/"
for app in $APPS; do
    rsync -a "$ROOT_DIR/static/packages/App/$app/" "$OUT/"
done
rsync -a "$ROOT_DIR/static/packages/App/Activity Tracker/" "$OUT/"
echo "$EMU_PACKAGES" | while read -r package; do
    if [ -n "$package" ]; then
        rsync -a "$ROOT_DIR/static/packages/Emu/$package/" "$OUT/"
    fi
done
"$ROOT_DIR/static/packages/common/apply.sh" "$OUT/Emu" > /dev/null

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

# RetroArch built by platform/rg35xx/retroarch/build.sh and cores.sh, and the
# GLSL shaders from shaders.sh
RA_DIR="$OUT/RetroArch/.retroarch"
mkdir -p "$RA_DIR/cores" "$RA_DIR/autoconfig" "$RA_DIR/shaders"
cp "$CACHE_DIR/retroarch/retroarch" "$OUT/RetroArch/retroarch"
cp "$CACHE_DIR"/cores/*_libretro.so "$RA_DIR/cores/"
cp -R "$PLATFORM_DIR/retroarch/autoconfig/." "$RA_DIR/autoconfig/"
cp -R "$CACHE_DIR/shaders/." "$RA_DIR/shaders/"

# Silky and the OnionUI/Themes remixed list (tools/get_themes.sh). As in the Miyoo build,
# the default Silky is extracted and the others stay zipped until the Theme
# Switcher installs them
THEMES_DIR="$CACHE_DIR/themes"
if [ -d "$THEMES_DIR" ]; then
    mkdir -p "$OUT/Themes"
    for zip in "$THEMES_DIR"/*.zip; do
        case "$zip" in
        */"Silky by DiMo.zip") unzip -oq "$zip" -d "$OUT/Themes" ;;
        *) cp "$zip" "$OUT/Themes/" ;;
        esac
    done
else
    echo "warning: no themes, run platform/rg35xx/tools/get_themes.sh" >&2
fi

# Onion's own MainUI replaces the Miyoo one
mv "$SYSDIR/bin/MainUI" "$OUT/miyoo/app/MainUI"

du -sh "$OUT"
