#!/bin/sh
# Saves what is on the RG35XX screen as a PNG on the host, over ADB.
# The framebuffer is 640x480, 32-bit BGRA; needs python3 with Pillow.
#
# Usage: screenshot.sh OUT.png
set -e

out="${1:?usage: $0 OUT.png}"
raw="$(mktemp)"
trap 'rm -f "$raw"' EXIT

adb shell "/system/bin/busybox dd if=/dev/fb0 of=/onion/tmp/fb.raw bs=2560 count=480 2>/dev/null"
adb pull /onion/tmp/fb.raw "$raw" > /dev/null
adb shell "rm /onion/tmp/fb.raw"

python3 -I - "$raw" "$out" << 'EOF'
import sys
from PIL import Image
raw = open(sys.argv[1], 'rb').read()
image = Image.frombuffer('RGBA', (640, 480), raw[:640 * 480 * 4], 'raw', 'BGRA', 0, 1)
image.convert('RGB').save(sys.argv[2])
EOF
echo "$out"
