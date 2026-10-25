#!/system/bin/sh
# Phase 0 hardware recon for the Anbernic RG35XX (original).
# Run on the device through ADB (create /misc/enableADB first):
#   adb push recon.sh /mnt/mmc/recon.sh
#   adb shell sh /mnt/mmc/recon.sh
#   adb pull /mnt/mmc/recon
# Read-only: it only reads sysfs/procfs and copies files into /mnt/mmc/recon.

OUT=/mnt/mmc/recon
mkdir -p $OUT

section() {
    echo
    echo "===== $1 ====="
}

dump() {
    for f in "$@"; do
        if [ -e "$f" ]; then
            echo "--- $f"
            cat "$f" 2>&1
        else
            echo "--- $f (missing)"
        fi
    done
}

{
    section "kernel"
    uname -a
    dump /proc/cmdline /proc/cpuinfo /proc/meminfo

    section "input"
    dump /proc/bus/input/devices
    ls -l /dev/input/
    ls -l /dev/uinput /dev/misc/uinput 2>&1

    section "framebuffer"
    ls -l /dev/fb* /dev/graphics/ 2>&1
    for f in /sys/class/graphics/fb0/*; do
        [ -f "$f" ] && dump "$f"
    done
    busybox fbset 2>&1

    section "backlight"
    for d in /sys/class/backlight/*; do
        dump "$d/brightness" "$d/max_brightness" "$d/bl_power"
    done

    section "power_supply"
    for d in /sys/class/power_supply/*; do
        ls "$d"
        for f in "$d"/*; do
            [ -f "$f" ] && dump "$f"
        done
    done

    section "audio"
    dump /proc/asound/cards /proc/asound/devices /proc/asound/pcm
    ls -l /dev/snd/ 2>&1
    which amixer tinymix 2>&1
    amixer scontrols 2>&1
    tinymix 2>&1
    find /sys -name volume -path '*gpio*' 2>/dev/null | while read f; do dump "$f"; done

    section "cpufreq"
    for f in /sys/devices/system/cpu/cpu0/cpufreq/*; do
        [ -f "$f" ] && dump "$f"
    done
    dump /sys/devices/system/cpu/autoplug/plug_mask

    section "power management"
    dump /sys/power/state /sys/power/wake_lock /sys/power/autosleep
    ls /sys/class/switch/ 2>&1
    dump /sys/class/switch/hdmi/state

    section "rtc"
    ls -l /dev/rtc* 2>&1

    section "mounts and block"
    cat /proc/mounts
    cat /proc/partitions
    ls -l /dev/block/

    section "modules"
    cat /proc/modules
    lsmod 2>&1
} > $OUT/recon.txt 2>&1

# Boot chain files from the stock read-only partitions
cp /init.rc /loadapp.sh $OUT/ 2>/dev/null
cp /system/dmenu/dmenu_ln $OUT/ 2>/dev/null
ls -lR /system/dmenu > $OUT/system_dmenu.txt 2>&1
ls -l /system/bin /system/lib > $OUT/system_bins.txt 2>&1

sync
echo "Done: $OUT"
