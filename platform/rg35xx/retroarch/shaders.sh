#!/bin/sh
# Collects the GLSL shader presets listed in shaders/presets.txt from
# libretro/glsl-shaders, with the files they use, rewritten for the PowerVR
# SGX GLSL ES 1.00 compiler (shaders/glsl_es_fix.py). Every listed preset
# runs at full speed on the RG35XX for the systems noted in presets.txt.
# Usage (from the repo root): platform/rg35xx/retroarch/shaders.sh
# Output: cache/rg35xx/shaders (copied to RetroArch/.retroarch/shaders by dist.sh)
set -e
GLSL_SHADERS_VERSION=cba01a48b0974a1232bea68d999a7c21bc4c233e
SHADERS_DIR="$(cd "$(dirname "$0")" && pwd)/shaders"
CACHE_DIR="$SHADERS_DIR/../../../../cache/rg35xx"
SRC="$CACHE_DIR/glsl-shaders"
OUT="$CACHE_DIR/shaders"

if [ "$(git -C "$SRC" rev-parse HEAD 2>/dev/null)" != $GLSL_SHADERS_VERSION ]; then
    rm -rf "$SRC"
    git init -q "$SRC"
    git -C "$SRC" fetch -q --depth 1 https://github.com/libretro/glsl-shaders.git $GLSL_SHADERS_VERSION
    git -C "$SRC" checkout -q FETCH_HEAD
fi

rm -rf "$OUT"
python3 -I "$SHADERS_DIR/collect.py" "$SRC" "$OUT" \
    $(sed 's/#.*//' "$SHADERS_DIR/presets.txt")
python3 -I "$SHADERS_DIR/glsl_es_fix.py" "$OUT" > /dev/null
echo "$(find "$OUT" -name '*.glslp' | wc -l | tr -d ' ') presets in $OUT"
