#!/bin/sh
# Builds libretro cores for the RG35XX (Cortex-A9, NEON, hard-float).
# Usage (from the repo root): platform/rg35xx/retroarch/cores.sh [core ...]
# Output: cache/rg35xx/cores/<name>_libretro.so, log in the Docker volume at
# /work/cores/<name>.log
set -e

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
CORES="${*:-gambatte fceumm snes9x2005 mednafen_supafaust gpsp mgba picodrive genesis_plus_gx pcsx_rearmed mednafen_pce_fast}"

mkdir -p "$ROOT_DIR/cache/rg35xx/cores"

"$ROOT_DIR/platform/rg35xx/tools/make.sh" "
    CORE_FLAGS='-marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard'
    export CC=\${CROSS_COMPILE}gcc CXX=\${CROSS_COMPILE}g++ AR=\${CROSS_COMPILE}ar
    mkdir -p /work/cores && cd /work/cores
    failed=''
    for core in $CORES; do
        # name repo makefile-dir make-arguments
        case \$core in
        gambatte)          repo=gambatte-libretro;     dir=.;  args='platform=unix' ;;
        fceumm)            repo=libretro-fceumm;       dir=.;  args='-f Makefile.libretro platform=unix' ;;
        snes9x2005)        repo=snes9x2005;            dir=.;  args='platform=unix' ;;
        gpsp)              repo=gpsp;                  dir=.;  args='platform=unix HAVE_DYNAREC=1 CPU_ARCH=arm MMAP_JIT_CACHE=1' ;;
        mgba)              repo=mgba;                  dir=.;  args='-f Makefile.libretro platform=unix' ;;
        picodrive)         repo=picodrive;             dir=.;  args='-f Makefile.libretro platform=unix ARCH=arm' ;;
        genesis_plus_gx)   repo=Genesis-Plus-GX;       dir=.;  args='-f Makefile.libretro platform=unix' ;;
        pcsx_rearmed)      repo=pcsx_rearmed;          dir=.;  args='-f Makefile.libretro platform=unix ARCH=arm DYNAREC=ari64 HAVE_NEON=1 BUILTIN_GPU=neon' ;;
        mednafen_pce_fast) repo=beetle-pce-fast-libretro; dir=.; args='platform=unix' ;;
        mednafen_supafaust) repo=supafaust;          dir=.;  args='platform=unix' ;;
        *) echo \"unknown core \$core\"; failed=\"\$failed \$core\"; continue ;;
        esac
        [ -d \$repo ] || git clone -q --depth 1 --recurse-submodules --shallow-submodules https://github.com/libretro/\$repo.git
        cd \$repo/\$dir
        make \$args clean > /dev/null 2>&1 || true
        # Flags go through the environment: on the command line they would
        # replace the core's own CFLAGS (include paths, defines)
        if CFLAGS=\"\$CORE_FLAGS\" CXXFLAGS=\"\$CORE_FLAGS\" make -j\$(nproc) \$args > /work/cores/\$core.log 2>&1; then
            so=\$(ls -t *_libretro.so | head -1)
            \${CROSS_COMPILE}strip -o /src/cache/rg35xx/cores/\${core}_libretro.so \$so
            echo \"OK   \$core\"
        else
            echo \"FAIL \$core (see /work/cores/\$core.log)\"; tail -5 /work/cores/\$core.log
            failed=\"\$failed \$core\"
        fi
        cd /work/cores
    done
    [ -z \"\$failed\" ] || echo \"failed:\$failed\"
"
ls -l "$ROOT_DIR/cache/rg35xx/cores"
