#!/usr/bin/env python3
"""Assembles a single page that carries both the WebAssembly build and the pure-JavaScript
build, and starts whichever the browser is allowed to run (some embedding hosts forbid
compiling WebAssembly). Output has no <html>/<head>/<body> tags so it can be wrapped by a host.

usage: make_artifact.py <wasm single-file html> <js single-file html> <out html>
"""
import re
import sys

def generated_script(path):
    html = open(path, encoding="utf-8").read()
    scripts = re.findall(r"<script(?: [^>]*)?>(.*?)</script>", html, flags=re.S)
    code = scripts[-1]  # the Emscripten output follows the shell's own Module script
    if "</script" in code.lower():
        raise SystemExit(path + ": generated code contains a closing script tag")
    # builds use -sSINGLE_FILE_BINARY_ENCODE=0 (base64), so the page is plain text
    bad = [c for c in ("\x00", "\ufffd") if c in code]
    if bad:
        raise SystemExit(path + ": generated code contains raw binary characters; build with -sSINGLE_FILE_BINARY_ENCODE=0")
    return code

def shell_parts(path):
    html = open(path, encoding="utf-8").read()
    style = re.search(r"<style>(.*?)</style>", html, flags=re.S).group(1)
    module = re.findall(r"<script>(.*?)</script>", html, flags=re.S)[0]
    return style, module

wasm_code = generated_script(sys.argv[1])
js_code = generated_script(sys.argv[2])
style, module = shell_parts(sys.argv[1])

page = f"""<title>The 1982 Crane</title>
<meta name="description" content="The 1982 Crane - a first-person horror game. Berth 9, Port of Ashgrove, November 11, 1982.">
<style>
  :root {{ --bg: #000000; --fg: #8b877a; color-scheme: dark; }}
  body {{ background: var(--bg); color: var(--fg); }}
{style}
</style>
<canvas id="canvas" tabindex="-1"></canvas>
<div id="status">THE 1982 CRANE<br>loading...</div>
<script>
{module}
</script>
<script type="text/plain" id="crane-build-wasm">{wasm_code}</script>
<script type="text/plain" id="crane-build-js">{js_code}</script>
<script>
  (function () {{
    var wasmOk = false;
    try {{
      // compiling even an empty module fails when the page is not allowed to use WebAssembly
      wasmOk = typeof WebAssembly === 'object' &&
        new WebAssembly.Module(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0])) instanceof WebAssembly.Module;
    }} catch (e) {{ wasmOk = false; }}
    window.craneBuild = wasmOk ? 'webassembly' : 'javascript';
    var src = document.getElementById(wasmOk ? 'crane-build-wasm' : 'crane-build-js');
    if (!wasmOk) {{
      var st = document.getElementById('status');
      st.innerHTML = 'THE 1982 CRANE<br>loading (compatibility mode, this takes a little longer)...';
    }}
    var s = document.createElement('script');
    s.id = 'mainScript'; // the generated code finds its own source by this id
    s.textContent = src.textContent;
    document.body.appendChild(s);
  }})();
</script>
"""
open(sys.argv[3], "w", encoding="utf-8").write(page)
print(sys.argv[3], len(page), "bytes")
