#!/bin/sh
# Copies to the card the files of cache/rg35xx/sd that differ from the ones
# there, safely while Onion runs: each file goes as FILE.new and is moved over
# the old one only once it is complete. Nothing is deleted. config.json and
# retroarch.cfg files are only copied when missing (Onion edits them on the
# device). Run with -n to only list.
#
# Usage (from the repo root): platform/rg35xx/tools/sync.sh [-n] [PATH ...]
#   PATH  relative to the card root; defaults to the Onion programs, scripts,
#         apps, emulator packages and RetroArch
set -e

REPO="$(cd "$(dirname "$0")/../../.." && pwd)"
SD="$REPO/cache/rg35xx/sd"
DRY=
[ "$1" = "-n" ] && { DRY=1; shift; }
PATHS="${*:-.tmp_update/bin .tmp_update/lib .tmp_update/script .tmp_update/runtime.sh miyoo/app App Emu RApp RetroArch}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

cd "$SD"
# shellcheck disable=SC2086
find $PATHS -type f 2> /dev/null | sort | while read -r f; do
    printf '%s  %s\n' "$(md5 -q "$f")" "$f"
done > "$TMP/host.md5"
[ -s "$TMP/host.md5" ] || { echo "nothing to compare" >&2; exit 1; }

# md5sum -c inside the chroot (the busybox of the stock system is too old)
adb push "$TMP/host.md5" /onion/tmp/host.md5 > /dev/null
"$REPO/platform/rg35xx/tools/devchroot.sh" shell \
    'cd /mnt/SDCARD && md5sum -c /tmp/host.md5 2>/dev/null | sed -n "s/: FAILED$//p" |
    while read -r f; do
        case "$f" in */config.json|*/retroarch.cfg) [ -e "$f" ] && continue ;; esac
        echo "$f"
    done; true' | tr -d '\r' > "$TMP/changed.txt"

if [ ! -s "$TMP/changed.txt" ]; then
    echo "card is up to date"
    exit 0
fi
cat "$TMP/changed.txt"
[ -z "$DRY" ] || exit 0

{
    echo 'B=/system/bin/busybox'
    while read -r f; do
        adb push "$f" "/mnt/mmc/$f.new" > /dev/null
        d=$(dirname "$f")
        echo "\$B mkdir -p '/mnt/mmc/$d' && [ -s '/mnt/mmc/$f.new' ] && \$B mv -f '/mnt/mmc/$f.new' '/mnt/mmc/$f' && chmod 755 '/mnt/mmc/$f' || echo 'FAILED $f'"
    done < "$TMP/changed.txt"
    echo 'sync; echo "applied"'
} > "$TMP/apply.sh"
adb push "$TMP/apply.sh" /data/local/tmp/apply.sh > /dev/null
adb shell "sh /data/local/tmp/apply.sh" | tr -d '\r'
