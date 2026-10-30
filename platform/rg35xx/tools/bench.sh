#!/bin/sh
# Measures a libretro core's speed on the device with no frame limit: audio
# and vsync off, saves, states and core options in /tmp, CPU clock fixed with
# the performance governor (RetroArch patch 0006 reads ./cpuclock.txt).
# RetroArch runs twice, for FRAMES/5 and FRAMES frames, and the FPS is taken
# from the difference, so loading the core and the ROM is not counted.
#
# Usage (from the repo root, Onion running with ADB):
#   platform/rg35xx/tools/bench.sh CORE ROM [FRAMES] [MHZ]
#   CORE  core name without _libretro.so (e.g. gpsp)
#   ROM   path on the card, as /mnt/SDCARD/Roms/...
#   defaults: 3000 frames, 1200 MHz
set -e

[ $# -ge 2 ] || { sed -n '2,13p' "$0"; exit 1; }
CORE="$1"
ROM="$2"
FRAMES="${3:-3000}"
MHZ="${4:-1200}"
REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# The script is written here with the values filled in, so ROM names with
# quotes or spaces do not go through the adb and chroot command lines
quote() { printf "'%s'" "$(printf %s "$1" | sed "s/'/'\\\\''/g")"; }
{
    echo "CORE=$(quote "$CORE")"
    echo "ROM=$(quote "$ROM")"
    echo "FRAMES=$FRAMES"
    echo "MHZ=$MHZ"
    cat << 'EOF'
trap '' HUP
D=/tmp/bench
CPU=/sys/devices/system/cpu/cpu0/cpufreq
rm -rf $D
mkdir -p $D/config $D/saves $D/states
echo $MHZ > $D/cpuclock.txt
cat > $D/bench.cfg << CFG
audio_driver = "null"
audio_sync = "false"
video_vsync = "false"
savefile_directory = "$D/saves"
savestate_directory = "$D/states"
rgui_config_directory = "$D/config"
core_options_path = "$D/options.cfg"
savestate_auto_load = "false"
savestate_auto_save = "false"
config_save_on_exit = "false"
network_cmd_enable = "false"
CFG
echo performance > $CPU/scaling_governor
cd $D
uptime_cs() { read up idle < /proc/uptime; echo ${up%.*}${up#*.}; }
run() {
    start=$(uptime_cs)
    HOME=/mnt/SDCARD/RetroArch /mnt/SDCARD/RetroArch/retroarch -v \
        -L /mnt/SDCARD/RetroArch/.retroarch/cores/${CORE}_libretro.so \
        --appendconfig $D/bench.cfg --max-frames $1 "$ROM" > $D/ra$1.log 2>&1 || true
    echo $(( $(uptime_cs) - start ))
}
short=$((FRAMES / 5))
t1=$(run $short)
t2=$(run $FRAMES)
grep -E '\[CPU\]|rror' $D/ra$FRAMES.log | grep -v -i 'no error' || true
echo "cur_freq $(cat $CPU/scaling_cur_freq)"
echo interactive > $CPU/scaling_governor
echo 1200000 > $CPU/scaling_max_freq
if [ $t2 -gt $t1 ]; then
    fps100=$(( (FRAMES - short) * 10000 / (t2 - t1) ))
    echo "RESULT $CORE ${MHZ}MHz: $((fps100 / 100)).$((fps100 % 100 / 10)) fps (${short} frames ${t1}cs, ${FRAMES} frames ${t2}cs)"
else
    echo "RESULT $CORE: no timing (${t1}cs, ${t2}cs), see $D/ra$FRAMES.log"
fi
EOF
} > "$TMP/bench_run.sh"

# The chroot's /tmp is a tmpfs at /onion/tmp
adb push "$TMP/bench_run.sh" /onion/tmp/bench_run.sh > /dev/null
"$REPO/platform/rg35xx/tools/devchroot.sh" run "sh /tmp/bench_run.sh"
