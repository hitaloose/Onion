#!/usr/bin/env python3
"""Copies the given .glslp presets from a libretro/glsl-shaders checkout, with
every file they use (shader passes, textures, #reference presets), keeping the
directory layout.
Usage: collect.py SRC_DIRS DEST_DIR PRESET...
SRC_DIRS is a ":"-separated list; each file is taken from the first directory
that has it, so a directory of extra presets can come before the checkout.
Presets are relative to the source directories."""
import os
import re
import shutil
import sys


def find(srcs, rel):
    for src in srcs:
        if os.path.isfile(os.path.join(src, rel)):
            return os.path.join(src, rel)
    return None


def preset_files(srcs, preset, seen):
    """Yields the paths (relative to src) a preset depends on, itself included."""
    if preset in seen:
        return
    seen.add(preset)
    yield preset
    base = os.path.dirname(preset)
    values = {}
    for line in open(find(srcs, preset), encoding="utf-8", errors="replace"):
        line = line.strip()
        ref = re.match(r'#reference\s+"?([^"]+)"?', line)
        if ref:
            yield from preset_files(srcs, os.path.normpath(os.path.join(base, ref.group(1))), seen)
            continue
        kv = re.match(r'([A-Za-z0-9_]+)\s*=\s*"?([^"]*)"?', line)
        if kv:
            values[kv.group(1)] = kv.group(2).strip()
    paths = [v for k, v in values.items() if re.fullmatch(r"shader\d+", k)]
    paths += [values[t] for t in values.get("textures", "").split(";") if t in values]
    for p in paths:
        yield os.path.normpath(os.path.join(base, p))


def main():
    srcs, dest, presets = sys.argv[1].split(":"), sys.argv[2], sys.argv[3:]
    seen = set()
    for preset in presets:
        if not find(srcs, preset):
            sys.exit(f"missing preset {preset}")
        for rel in preset_files(srcs, preset, seen):
            path = find(srcs, rel)
            if not path:
                sys.exit(f"{preset}: missing {rel}")
            os.makedirs(os.path.join(dest, os.path.dirname(rel)), exist_ok=True)
            shutil.copy(path, os.path.join(dest, rel))
    print(f"{len(presets)} presets")


main()
