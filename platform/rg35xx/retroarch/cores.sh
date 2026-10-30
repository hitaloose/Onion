#!/bin/sh
# Builds libretro cores for the RG35XX (Cortex-A9, NEON, hard-float).
# Usage (from the repo root): platform/rg35xx/retroarch/cores.sh [core ...]
# Output: cache/rg35xx/cores/<name>_libretro.so, log in the Docker volume at
# /work/cores/<name>.log
set -e

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
CORES="${*:-gambatte fceumm snes9x2005 mednafen_supafaust gpsp mgba picodrive genesis_plus_gx pcsx_rearmed mednafen_pce_fast
    mame2003_plus fbalpha2012_cps1 fbalpha2012_cps2 fbalpha2012_neogeo}"

mkdir -p "$ROOT_DIR/cache/rg35xx/cores"

"$ROOT_DIR/platform/rg35xx/tools/make.sh" "
    CORE_FLAGS='-marm -mcpu=cortex-a9 -mfpu=neon -mfloat-abi=hard'
    export CC=\${CROSS_COMPILE}gcc CXX=\${CROSS_COMPILE}g++ AR=\${CROSS_COMPILE}ar
    mkdir -p /work/cores && cd /work/cores
    failed=''
    for core in $CORES; do
        # name repo makefile-dir make-arguments (cmake arguments with build=cmake)
        build=make
        case \$core in
        gambatte)          repo=gambatte-libretro;     dir=.;  args='platform=unix' ;;
        fceumm)            repo=libretro-fceumm;       dir=.;  args='-f Makefile.libretro platform=unix' ;;
        snes9x2005)        repo=snes9x2005;            dir=.;  args='platform=unix' ;;
        gpsp)              repo=gpsp;                  dir=.;  args='platform=unix HAVE_DYNAREC=1 CPU_ARCH=arm MMAP_JIT_CACHE=1' ;;
        mgba)              repo=mgba;                  dir=.;  build=cmake; args='-DLIBMGBA_ONLY=ON -DBUILD_LIBRETRO=ON' ;;
        picodrive)         repo=picodrive;             dir=.;  args='-f Makefile.libretro platform=unix ARCH=arm' ;;
        genesis_plus_gx)   repo=Genesis-Plus-GX;       dir=.;  args='-f Makefile.libretro platform=unix' ;;
        pcsx_rearmed)      repo=pcsx_rearmed;          dir=.;  args='-f Makefile.libretro platform=unix ARCH=arm DYNAREC=ari64 HAVE_NEON=1 BUILTIN_GPU=neon' ;;
        mednafen_pce_fast) repo=beetle-pce-fast-libretro; dir=.; args='platform=unix' ;;
        mednafen_supafaust) repo=supafaust;          dir=.;  args='platform=unix' ;;
        mame2003_plus)     repo=mame2003-plus-libretro; dir=.; args='platform=unix' ;;
        fbalpha2012_cps1)  repo=fbalpha2012_cps1;       dir=.; args='-f makefile.libretro platform=unix' ;;
        fbalpha2012_cps2)  repo=fbalpha2012_cps2;       dir=.; args='-f makefile.libretro platform=unix' ;;
        fbalpha2012_neogeo) repo=fbalpha2012_neogeo;    dir=.; args='-f makefile.libretro platform=unix' ;;
        *) echo \"unknown core \$core\"; failed=\"\$failed \$core\"; continue ;;
        esac
        [ -d \$repo ] || git clone -q --depth 1 --recurse-submodules --shallow-submodules https://github.com/libretro/\$repo.git
        cd \$repo/\$dir
        if [ \$build = cmake ]; then
            # Buildroot's toolchain file: compiler, sysroot and find paths
            rm -rf build-rg35xx
            built() {
                cmake -S . -B build-rg35xx -G Ninja \\
                    -DCMAKE_TOOLCHAIN_FILE=/work/sdk/share/buildroot/toolchainfile.cmake \\
                    -DCMAKE_BUILD_TYPE=Release \\
                    -DCMAKE_C_FLAGS=\"-O2 \$CORE_FLAGS\" -DCMAKE_CXX_FLAGS=\"-O2 \$CORE_FLAGS\" \\
                    \$args && cmake --build build-rg35xx -j\$(nproc)
            }
            sodir=build-rg35xx
        else
            make \$args clean > /dev/null 2>&1 || true
            # Flags go through the environment: on the command line they would
            # replace the core's own CFLAGS (include paths, defines)
            built() {
                CFLAGS=\"\$CORE_FLAGS\" CXXFLAGS=\"\$CORE_FLAGS\" make -j\$(nproc) \$args
            }
            sodir=.
        fi
        if built > /work/cores/\$core.log 2>&1; then
            so=\$(ls -t \$sodir/*_libretro.so | head -1)
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
