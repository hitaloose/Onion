#!/bin/sh
# Development helper: runs the Onion rootfs in a chroot on a RG35XX that is
# still booting another firmware (e.g. Garlic with /misc/enableADB), over ADB.
#
# Usage (from the repo root):
#   devchroot.sh push-rootfs   copy cache/rg35xx/rootfs.img (refuses while mounted)
#   devchroot.sh push-sd       copy cache/rg35xx/sd/ to the roms partition
#   devchroot.sh mount         stop the running launcher, mount and start padmapd
#   devchroot.sh umount        stop Onion processes and unmount everything
#   devchroot.sh run CMD...    run CMD detached from the adb pty (SDL needs the VT)
#                              and print its output when it exits
#   devchroot.sh shell CMD...  run CMD in the chroot attached to adb (no SDL)
#   devchroot.sh reboot        reboot through Onion's shutdown script, which
#                              remounts the card read-only (adb reboot leaves
#                              the FAT partition dirty)
set -e

ROOT=/onion
LOOP=/dev/block/loop6
IMAGE=/mnt/mmc/.tmp_update/rootfs.img
B=/system/bin/busybox
CHROOT="$B chroot $ROOT /usr/bin/env PATH=/usr/sbin:/usr/bin:/sbin:/bin HOME=/mnt/SDCARD SDL_NOMOUSE=1 LD_LIBRARY_PATH=/mnt/SDCARD/.tmp_update/lib"
REPO="$(cd "$(dirname "$0")/../../.." && pwd)"

is_mounted() {
    adb shell "$B grep -q ' $ROOT ' /proc/mounts && echo yes" | grep -q yes
}

case "$1" in
push-rootfs)
    if is_mounted; then
        echo "rootfs is mounted, run '$0 umount' first" >&2
        exit 1
    fi
    adb shell "mkdir -p /mnt/mmc/.tmp_update"
    adb push "$REPO/cache/rg35xx/rootfs.img" $IMAGE
    ;;
push-sd)
    adb push "$REPO/cache/rg35xx/sd/." /mnt/mmc/
    ;;
mount)
    adb shell "
        for p in \$($B ps w | $B grep -E 'CFW/start|\./main' | $B grep -v grep | $B awk '{print \$1}'); do kill \$p; done
        mkdir -p $ROOT
        /mnt/mmc/.tmp_update/bin/loopro $LOOP $IMAGE
        mount -r -t ext4 $LOOP $ROOT
        for f in tmp run appconfigs; do mount -t tmpfs tmpfs $ROOT/\$f; done
        for f in dev dev/pts proc sys misc; do mount -o bind /\$f $ROOT/\$f; done
        mount -o bind /mnt/mmc $ROOT/mnt/SDCARD
        $CHROOT /bin/sh -c 'nohup /mnt/SDCARD/.tmp_update/bin/padmapd >/dev/null 2>&1 </dev/null &'
        sleep 1
        ls /dev/input/onion-kbd"
    ;;
umount)
    adb shell "
        for p in \$($B ps w | $B grep -E '/mnt/SDCARD/' | $B grep -v grep | $B awk '{print \$1}'); do kill \$p; done
        sleep 1
        rm -f /dev/input/onion-kbd
        for m in mnt/SDCARD misc sys proc dev/pts dev appconfigs run tmp; do umount $ROOT/\$m; done
        umount $ROOT && $B losetup -d $LOOP
        $B grep ' $ROOT' /proc/mounts || echo unmounted"
    ;;
run)
    shift
    adb shell "$B setsid $CHROOT nohup /bin/sh -c '$*' > /mnt/mmc/.tmp_update/devchroot.log 2>&1 < /dev/null & wait \$!; echo exit=\$?; cat /mnt/mmc/.tmp_update/devchroot.log"
    ;;
reboot)
    # A background job dies with the adb session; the reboot ends the wait
    adb shell "$B setsid $CHROOT /mnt/SDCARD/.tmp_update/bin/shutdown -r > /dev/null 2>&1 < /dev/null & wait \$!"
    ;;
shell)
    shift
    adb shell "$CHROOT /bin/sh -c '$*'"
    ;;
*)
    sed -n '2,15p' "$0"
    exit 1
    ;;
esac
