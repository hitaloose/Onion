#!/system/bin/sh
# Development reboot that leaves the card clean: stops Onion, unmounts the
# chroot, remounts the card read-only and reboots. Runs in the stock system
# from /tmp (not from the card, which it remounts read-only).
B=/system/bin/busybox
R=/onion

for p in $($B ps w | $B grep -E "updater|runtime.sh|MainUI|keymon|batmon|padmapd|chargingState|retroarch|infoPanel|gameSwitcher" | $B grep -v grep | $B awk '{print $1}'); do
    [ "$p" = "$$" ] || kill -9 $p
done
sleep 1

swapoff $R/mnt/SDCARD/cachefile 2>/dev/null
for m in mnt/SDCARD2 mnt/SDCARD misc sys proc dev/pts dev etc/group etc/passwd appconfigs run tmp; do
    $B grep -q " $R/$m " /proc/mounts && umount $R/$m
done
umount $R
$B losetup -d /dev/block/loop7 2>/dev/null

sync
if mount -o remount,ro /mnt/mmc; then
    echo "card read-only, rebooting"
    $B reboot -f
else
    echo "card still busy, not rebooting"
    $B grep -E " $R| /mnt/mmc " /proc/mounts
fi
