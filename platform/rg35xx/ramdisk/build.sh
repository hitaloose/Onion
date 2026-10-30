#!/bin/sh
# Builds the RG35XX boot ramdisk: the stock misc/ramdisk.img (Anbernic init,
# charger and scripts) with init.rc and bootmode.sh from this directory.
# Usage (from the repo root): platform/rg35xx/ramdisk/build.sh <stock ramdisk.img>
#   The stock image is in the misc partition of a stock or Garlic card, and in
#   garlic-misc/ on the roms partition of the test card.
# Output: cache/rg35xx/ramdisk.img, a U-Boot legacy image like the stock one
#         (type ramdisk, uncompressed newc cpio, load address 0x02000000)
#         cache/rg35xx/boot_logo.bmp.gz, U-Boot's logo, from Onion's boot screen
set -e
RAMDISK_DIR="$(cd "$(dirname "$0")" && pwd)"
OUT="$RAMDISK_DIR/../../../cache/rg35xx/ramdisk.img"
STOCK="$1"
[ -f "$STOCK" ] || { echo "usage: $0 <stock ramdisk.img>"; exit 1; }

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# Unpack: 64-byte U-Boot header, then the cpio archive
mkdir "$WORK/root"
dd if="$STOCK" of="$WORK/stock.cpio" bs=64 skip=1 2> /dev/null
(cd "$WORK/root" && bsdtar -xf ../stock.cpio)

install -m 755 "$RAMDISK_DIR/init.rc" "$RAMDISK_DIR/bootmode.sh" "$WORK/root/"

# The roms partition with errors=continue like Garlic: the kernel default,
# remount-ro, leaves the card read-only until reboot after any FAT error, and
# a remount does not change it (fat_remount ignores the options)
sed -i.orig 's|-o rw,utf8,noatime,nodiratime /dev/block/mmcblk0p4|-o rw,utf8,noatime,nodiratime,errors=continue /dev/block/mmcblk0p4|' \
    "$WORK/root/loadapp.sh"
rm "$WORK/root/loadapp.sh.orig"
grep -q 'errors=continue /dev/block/mmcblk0p4' "$WORK/root/loadapp.sh"

# Onion's boot screen as /initlogo.rle, which init draws on the framebuffer.
# Without it init writes "A N D R O I D" on the console instead
python3 "$RAMDISK_DIR/../tools/bootlogo.py" rle "$RAMDISK_DIR/../../../src/bootScreen/res/bootScreen.png" \
    "$WORK/root/initlogo.rle"

# Entries without a leading "./", owned by root, as the kernel expects
(cd "$WORK/root" && bsdtar -cf ../ramdisk.cpio --format newc --uid 0 --gid 0 $(ls -A))

mkdir -p "$(dirname "$OUT")"
python3 - "$WORK/ramdisk.cpio" "$OUT" << 'EOF'
import struct, sys, time, zlib
data = open(sys.argv[1], 'rb').read()
IH_OS_LINUX, IH_ARCH_ARM, IH_TYPE_RAMDISK, IH_COMP_NONE = 5, 2, 3, 0
LOAD = 0x02000000
now = int(time.time())
def header(hcrc):
    return struct.pack('>7I4B32s', 0x27051956, hcrc, now, len(data),
                       LOAD, LOAD, zlib.crc32(data) & 0xffffffff,
                       IH_OS_LINUX, IH_ARCH_ARM, IH_TYPE_RAMDISK, IH_COMP_NONE, b'RAMFS')
hcrc = zlib.crc32(header(0)) & 0xffffffff
open(sys.argv[2], 'wb').write(header(hcrc) + data)
EOF
echo "$OUT: $(wc -c < "$OUT" | tr -d ' ') bytes"

# U-Boot's boot logo for the misc partition, the same image
LOGO="$(dirname "$OUT")/boot_logo.bmp.gz"
python3 "$RAMDISK_DIR/../tools/bootlogo.py" bmp "$RAMDISK_DIR/../../../src/bootScreen/res/bootScreen.png" "$WORK/logo.bmp"
gzip -9 -n -c "$WORK/logo.bmp" > "$LOGO"
echo "$LOGO: $(wc -c < "$LOGO" | tr -d ' ') bytes"
