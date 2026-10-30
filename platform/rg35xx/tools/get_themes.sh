#!/bin/sh
# Downloads every theme of the OnionUI/Themes repository (release/*.zip) into
# cache/rg35xx/themes. Already downloaded zips are kept, so running it again
# only fetches new themes. The Miyoo build (.github/get_themes.sh) takes only
# the featured ones.
# Usage (from the repo root): platform/rg35xx/tools/get_themes.sh
set -e

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
DEST="$ROOT_DIR/cache/rg35xx/themes"
REPO="https://github.com/OnionUI/Themes/raw/main/release"
mkdir -p "$DEST"

curl -fsSL "https://api.github.com/repos/OnionUI/Themes/contents/release" |
    python3 -I -c 'import json, sys
for f in json.load(sys.stdin):
    if f["name"].endswith(".zip"):
        print(f["name"])' > "$DEST/.list"

while read -r zip; do
    [ -f "$DEST/$zip" ] && continue
    echo "-- downloading theme: ${zip%.zip}"
    url="$REPO/$(python3 -I -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1]))' "$zip")"
    curl -fsSL -o "$DEST/$zip.part" "$url"
    mv "$DEST/$zip.part" "$DEST/$zip"
done < "$DEST/.list"

echo "$(wc -l < "$DEST/.list" | tr -d ' ') themes in $DEST"
