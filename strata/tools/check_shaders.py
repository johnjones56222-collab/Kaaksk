#!/usr/bin/env python3
"""Validates every GLSL program for OpenGL 3.3 core and GLSL ES 3.00 (WebGL 2).

Usage: check_shaders.py <path to strata_shader_dump>

The dump tool writes each program's vertex and fragment stage, assembled exactly
as the engine does, for both dialects. glslangValidator (package glslang-tools)
then compiles and links each pair, which catches desktop-only GLSL (implicit
int to float conversions, missing precision qualifiers, ...) before it reaches
a phone.
"""
import pathlib
import shutil
import subprocess
import sys
import tempfile


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    validator = shutil.which("glslangValidator")
    if not validator:
        print("glslangValidator not found (install glslang-tools)")
        return 2
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([sys.argv[1], tmp], check=True, stdout=subprocess.DEVNULL)
        verts = sorted(pathlib.Path(tmp).glob("*.vert"))
        failures = 0
        for vert in verts:
            frag = vert.with_suffix(".frag")
            result = subprocess.run([validator, "-l", str(vert), str(frag)], capture_output=True, text=True)
            name = vert.name[: -len(".vert")]
            if result.returncode != 0:
                failures += 1
                print(f"FAIL {name}")
                for line in result.stdout.splitlines():
                    if line.strip() and not line.endswith((".vert", ".frag")):
                        print("   ", line)
            else:
                print(f"ok   {name}")
        print(f"{len(verts) - failures}/{len(verts)} programs valid")
        return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
