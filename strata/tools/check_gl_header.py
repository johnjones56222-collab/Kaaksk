#!/usr/bin/env python3
"""Checks src/gl/gl.hpp against the Khronos glcorearb.h header.

Every constant must have the same value and every function the same return
and parameter types as the official header. Run from the repository root:

    python3 tools/check_gl_header.py [/usr/include/GL/glcorearb.h]
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OURS = ROOT / "src" / "gl" / "gl.hpp"


def strip_param_name(param: str) -> str:
    param = param.strip()
    if param in ("", "void"):
        return param
    # Remove the trailing identifier (the parameter name).
    m = re.match(r"^(.*?)([A-Za-z_][A-Za-z0-9_]*)$", param)
    if m and m.group(1).strip() and not m.group(1).strip().endswith(("const", "unsigned")):
        return m.group(1)
    if m and m.group(1).strip().endswith("*"):
        return m.group(1)
    return param


def norm_type(t: str) -> str:
    return re.sub(r"\s+", "", t)


def split_params(params: str):
    params = params.strip()
    if params.startswith("(") and params.endswith(")"):
        params = params[1:-1]
    if params.strip() in ("", "void"):
        return ["void"]
    return [norm_type(strip_param_name(p)) for p in params.split(",")]


def parse_official(path: Path):
    text = path.read_text()
    consts = {}
    for m in re.finditer(r"^#define\s+(GL_[A-Za-z0-9_]+)\s+(0x[0-9A-Fa-f]+|\d+)\s*$", text, re.M):
        consts.setdefault(m.group(1), int(m.group(2), 0))
    funcs = {}
    for m in re.finditer(r"GLAPI\s+(.+?)\s*APIENTRY\s+(gl\w+)\s*\((.*?)\);", text):
        funcs[m.group(2)] = (norm_type(m.group(1)), split_params(m.group(3)))
    return consts, funcs


def balanced_groups(s: str):
    """Yields the argument strings of every X(...) invocation in s."""
    i = 0
    while True:
        i = s.find("X(", i)
        if i < 0:
            return
        depth = 0
        start = i + 1
        for j in range(start, len(s)):
            if s[j] == "(":
                depth += 1
            elif s[j] == ")":
                depth -= 1
                if depth == 0:
                    yield s[start + 1 : j]
                    i = j
                    break


def split_top_level(s: str):
    parts, depth, cur = [], 0, ""
    for ch in s:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
    parts.append(cur)
    return [p.strip() for p in parts]


def parse_ours(path: Path):
    text = path.read_text()
    consts = {}
    for m in re.finditer(r"^#define\s+(GL_[A-Za-z0-9_]+)\s+(0x[0-9A-Fa-f]+|\d+)\s*$", text, re.M):
        consts[m.group(1)] = int(m.group(2), 0)
    joined = text.replace("\\\n", " ")
    funcs = {}
    for block_name in ("STRATA_GL_FUNCTIONS", "STRATA_GL_OPTIONAL_FUNCTIONS"):
        m = re.search(r"#define\s+" + block_name + r"\(X\)(.*)", joined)
        if not m:
            raise SystemExit(f"could not find {block_name}")
        body = m.group(1)
        for group in balanced_groups(body):
            ret, name, params, _args = split_top_level(group)
            funcs[name] = (norm_type(ret), split_params(params))
    return consts, funcs


def main():
    official_path = Path(sys.argv[1] if len(sys.argv) > 1 else "/usr/include/GL/glcorearb.h")
    off_consts, off_funcs = parse_official(official_path)
    our_consts, our_funcs = parse_ours(OURS)
    errors = 0
    for name, value in sorted(our_consts.items()):
        if name not in off_consts:
            print(f"constant {name} not in official header")
            errors += 1
        elif off_consts[name] != value:
            print(f"constant {name}: ours {value:#x}, official {off_consts[name]:#x}")
            errors += 1
    for name, (ret, params) in sorted(our_funcs.items()):
        if name not in off_funcs:
            print(f"function {name} not in official header")
            errors += 1
            continue
        oret, oparams = off_funcs[name]
        if ret != oret or params != oparams:
            print(f"function {name}: ours {ret} {params}, official {oret} {oparams}")
            errors += 1
    print(f"checked {len(our_consts)} constants and {len(our_funcs)} functions: {errors} problem(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
