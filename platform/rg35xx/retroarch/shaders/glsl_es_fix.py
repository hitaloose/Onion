#!/usr/bin/env python3
"""Rewrites libretro GLSL shaders that the PowerVR SGX GLSL ES 1.00 compiler
rejects, in place:
- a second `#define COMPAT_PRECISION` is a redefinition error there (other
  compilers only warn): each one is preceded by `#undef COMPAT_PRECISION`
- GLSL ES 1.00 has no non-square matrices: `mul(w, mat4x3(a, b, c, d))`
  (RetroArch's mul(a, b) is b * a) becomes rg_m43_v4(a, b, c, d, w), and
  `mul(mat4x3(a, b, c, d), v)` becomes rg_v3_m43(a, b, c, d, v), with both
  helpers defined before the fragment shader's main()
Usage: glsl_es_fix.py DIR  (every .glsl file under DIR)"""
import os
import re
import sys

HELPERS = """// GLSL ES 1.00 has no mat4x3 (added for the RG35XX PowerVR GPU)
vec3 rg_m43_v4(vec3 a, vec3 b, vec3 c, vec3 d, vec4 w)
{
    return w.x * a + w.y * b + w.z * c + w.w * d;
}
vec4 rg_v3_m43(vec3 a, vec3 b, vec3 c, vec3 d, vec3 v)
{
    return vec4(dot(a, v), dot(b, v), dot(c, v), dot(d, v));
}

"""

def split_call(source, start):
    """Parses the call whose "(" is at start: returns (arguments, end), the
    top-level comma-separated arguments and the index after ")"."""
    depth, args, current = 0, [], start + 1
    for i in range(start, len(source)):
        c = source[i]
        if c == "(":
            depth += 1
        elif c == ")":
            depth -= 1
            if depth == 0:
                args.append(source[current:i].strip())
                return args, i + 1
        elif c == "," and depth == 1:
            args.append(source[current:i].strip())
            current = i + 1
    raise ValueError("unbalanced parentheses")


def rewrite_mat4x3(source):
    """Rewrites every mul() that has a mat4x3(...) argument."""
    pos = 0
    while True:
        start = source.find("mul(", pos)
        if start < 0:
            return source
        if start > 0 and (source[start - 1].isalnum() or source[start - 1] == "_"):
            pos = start + 4
            continue
        args, end = split_call(source, start + 3)
        if len(args) == 2 and args[1].startswith("mat4x3("):
            columns, _ = split_call(args[1], 6)
            call = "rg_m43_v4(%s, %s)" % (", ".join(columns), args[0])
        elif len(args) == 2 and args[0].startswith("mat4x3("):
            columns, _ = split_call(args[0], 6)
            call = "rg_v3_m43(%s, %s)" % (", ".join(columns), args[1])
        else:
            pos = start + 4
            continue
        source = source[:start] + call + source[end:]
        pos = start + len(call)


def fix(source):
    source = re.sub(r"^([ \t]*)#define COMPAT_PRECISION\b",
                    r"\1#undef COMPAT_PRECISION\n\1#define COMPAT_PRECISION",
                    source, flags=re.M)
    if "mat4x3" not in source:
        return source
    source = rewrite_mat4x3(source)
    if "mat4x3" in source:
        raise ValueError("mat4x3 left that this script cannot rewrite")
    main = source.rfind("void main")
    return source[:main] + HELPERS + source[main:]


def main():
    for root, _, files in os.walk(sys.argv[1]):
        for name in files:
            if not name.endswith(".glsl"):
                continue
            path = os.path.join(root, name)
            source = open(path, encoding="utf-8", errors="replace").read()
            try:
                fixed = fix(source)
            except ValueError as error:
                sys.exit(f"{path}: {error}")
            if fixed != source:
                open(path, "w", encoding="utf-8").write(fixed)
                print(f"fixed {os.path.relpath(path, sys.argv[1])}")


main()
