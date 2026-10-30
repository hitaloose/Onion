#!/system/bin/sh
# /tmp/flaglog.sh - records the DIS_MCHRG pstore flag once every 2s
B=/system/bin/busybox
D=/sys/devices/b0174000.i2c/i2c-1/1-0065
while :; do
    echo "$($B cut -d' ' -f1 /proc/uptime) $($B cat $D/pstore_dbg 2>/dev/null | $B grep DIS_MCHRG)" >> /mnt/data/flag.log
    $B sync
    $B sleep 2
done
