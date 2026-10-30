#!/bin/sh
# Downloads the themes shipped on the RG35XX card into cache/rg35xx/themes:
# Silky by DiMo (the default) and the "remixed" list of the OnionUI/Themes
# repository (.github/data/remixed.txt, 15 themes). All 202 themes made the
# Theme Switcher take minutes to open. Already downloaded zips are kept; zips
# no longer in the list are removed.
# Usage (from the repo root): platform/rg35xx/tools/get_themes.sh
set -e

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
DEST="$ROOT_DIR/cache/rg35xx/themes"
RAW="https://raw.githubusercontent.com/OnionUI/Themes/main"
mkdir -p "$DEST"

{
    echo "Silky by DiMo"
    curl -fsSL "$RAW/.github/data/remixed.txt"
} | sed '/^$/d' > "$DEST/.list"

for zip in "$DEST"/*.zip; do
    [ -f "$zip" ] || continue
    grep -qxF "$(basename "$zip" .zip)" "$DEST/.list" || rm -f "$zip"
done

while read -r theme; do
    [ -f "$DEST/$theme.zip" ] && continue
    echo "-- downloading theme: $theme"
    url="$RAW/release/$(python3 -I -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1]))' "$theme.zip")"
    curl -fsSL -o "$DEST/$theme.zip.part" "$url"
    mv "$DEST/$theme.zip.part" "$DEST/$theme.zip"
done < "$DEST/.list"

echo "$(wc -l < "$DEST/.list" | tr -d ' ') themes in $DEST"
