#!/system/bin/sh
# Development helper: mirrors the kernel log to the card, one sync per line,
# so the last messages before a reboot, a crash or a power-off survive it.
# Runs in the stock system (not in the chroot), started over ADB:
#   adb push platform/rg35xx/tools/kmsglog.sh /tmp/kmsglog.sh
#   adb shell 'setsid nohup sh /tmp/kmsglog.sh > /dev/null 2>&1 < /dev/null &'
# Read the result in .tmp_update/logs/kmsg.log after the device is back.
# KMSGLOG_OUT redirects the log (e.g. to /mnt/data/kmsg.log, which survives the
# card being remounted read-only just before a reboot).
OUT=${KMSGLOG_OUT:-/mnt/mmc/.tmp_update/logs/kmsg.log}
LOG=${KMSGLOG_LOG:-/mnt/mmc/.tmp_update/logs/kmsglog.log}
B=/system/bin/busybox

mkdir -p "$($B dirname "$OUT")" "$($B dirname "$LOG")"
echo "=== kmsglog start $($B date) uptime $($B cut -d' ' -f1 /proc/uptime) ===" >> "$LOG"
echo "=== kmsglog start $($B date) uptime $($B cut -d' ' -f1 /proc/uptime) ===" >> "$OUT"

while :; do
    while read -r line; do
        echo "$line" >> "$OUT"
        sync
    done < /proc/kmsg
    sleep 1
done
