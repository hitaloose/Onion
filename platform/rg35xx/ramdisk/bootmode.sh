#!/system/bin/busybox sh
# Runs in charger mode and picks between the charger screen and the system.
# U-Boot shows the charger whenever a cable is plugged in: the kernel's "skip
# the charger" flag (DIS_MCHRG) lives in a PMIC register that the reboot
# reset clears, so reboots (ours and the charger's own power-key reboot) land
# here too. The PMIC still records what woke the device and the kernel logs
# it at boot: only a cable plug (WALL_IN, VBUS_IN) keeps the charger screen.
# Without that line the charger screen stays, as on the stock ramdisk.
B=/system/bin/busybox
WALL_IN=0x200
VBUS_IN=0x400

flags=$(/system/bin/dmesg | $B sed -n 's/.*translated wakeup falgs: \(0x[0-9a-f]*\).*/\1/p')
if [ -n "$flags" ] && [ $((flags & (WALL_IN | VBUS_IN))) -eq 0 ]; then
    mode=system
else
    mode=charger
fi

echo "$($B cut -d' ' -f1 /proc/uptime) wakeup=${flags:-none} mode=$mode" >> /mnt/data/bootmode.log

# Debugging aid: charger mode has no ADB, so mirror the kernel log (which
# /charger writes to) to /mnt/data while /mnt/data/.chargerlog exists
if [ $mode = charger ] && [ -f /mnt/data/.chargerlog ]; then
    /system/bin/dmesg > /mnt/data/charger-kmsg.log
    (while read -r line; do
        echo "$line" >> /mnt/data/charger-kmsg.log
        sync
    done < /proc/kmsg) &
fi
/system/bin/setprop sys.onion.boot $mode
