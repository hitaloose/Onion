#!/bin/sh
# Mount points used by misc/dmenu.bin when it binds the stock system into the chroot
set -e
TARGET_DIR="$1"
mkdir -p "$TARGET_DIR/misc" "$TARGET_DIR/mnt/SDCARD" "$TARGET_DIR/mnt/SDCARD2" "$TARGET_DIR/dev/pts"
