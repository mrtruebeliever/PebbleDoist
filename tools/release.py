#!/usr/bin/env python3
"""Builds a size-optimized release .pbw for the Pebble appstore.

`pebble build` ships an unminified webpack bundle plus its source map, which
together are ~90% of the .pbw. This script:
  - minifies the JavaScript bundle (pebble-js-app.js) with terser,
  - drops the source map (pebble-js-app.js.map) and its reference.
Everything else (app binary, resources, manifest, appinfo) is copied byte-for-byte,
so the per-platform manifest checksums (which only cover the binary + resources)
stay valid. Entries are stored uncompressed, matching how `pebble build` writes the
.pbw.

Requires node/npx (terser is fetched on first run, so the first run needs network).

Usage:  python3 tools/release.py      # runs `pebble build` first
Output: build/PebbleDoist-release.pbw # upload THIS to the store
"""
import os
import sys
import subprocess
import tempfile
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "build", "PebbleDoist.pbw")
OUT = os.path.join(ROOT, "build", "PebbleDoist-release.pbw")
JS = "pebble-js-app.js"
MAP = "pebble-js-app.js.map"


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=ROOT, check=True, **kw)


def main():
    run(["pebble", "build"])
    if not os.path.exists(SRC):
        sys.exit("build failed: %s not found" % SRC)

    zin = zipfile.ZipFile(SRC)
    js_src = zin.read(JS)

    # Minify (compress + mangle); terser also strips the sourceMappingURL comment.
    with tempfile.NamedTemporaryFile("wb", suffix=".js", delete=False) as tf:
        tf.write(js_src)
        tmp_in = tf.name
    try:
        p = subprocess.run(["npx", "--yes", "terser", tmp_in, "-c", "-m"],
                           stdout=subprocess.PIPE, cwd=ROOT)
    finally:
        os.unlink(tmp_in)
    if p.returncode != 0 or not p.stdout:
        sys.exit("terser failed (returncode %s)" % p.returncode)
    js_min = p.stdout

    # Sanity check: the minified bundle must still parse.
    with tempfile.NamedTemporaryFile("wb", suffix=".js", delete=False) as tf:
        tf.write(js_min)
        tmp_out = tf.name
    try:
        run(["node", "--check", tmp_out])
    finally:
        os.unlink(tmp_out)

    with zipfile.ZipFile(OUT, "w", zipfile.ZIP_STORED) as zout:
        for item in zin.infolist():
            if item.filename == MAP:
                continue  # drop the source map
            data = js_min if item.filename == JS else zin.read(item.filename)
            zout.writestr(item.filename, data)
    zin.close()

    src_sz, out_sz = os.path.getsize(SRC), os.path.getsize(OUT)
    print("pebble-js-app.js: %d -> %d bytes" % (len(js_src), len(js_min)))
    print("pbw: %d -> %d bytes (%.0f%% smaller)" % (src_sz, out_sz, 100 * (1 - out_sz / src_sz)))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
