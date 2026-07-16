#!/usr/bin/env python3
"""Builds a size-optimized release .pbw for the Pebble appstore.

Project-agnostic: run it from any Pebble project's root (`python3 tools/release.py`).
`pebble build` ships an unminified webpack bundle plus its source map, which together
are ~90% of the .pbw. This script:
  - minifies the JavaScript bundle (pebble-js-app.js) with terser,
  - drops the source map (pebble-js-app.js.map) and its reference.
Everything else (app binary, resources, manifest, appinfo) is copied byte-for-byte,
so the per-platform manifest checksums (which only cover the binary + resources)
stay valid. Entries are stored uncompressed, matching how `pebble build` writes the
.pbw. A project with no phone-side JS simply gets the source map dropped.

Requires node/npx for the minify step (terser is fetched on first run, so the first
run needs network).

Usage:  python3 tools/release.py
Output: build/<AppName>-release.pbw   # upload THIS to the store
"""
import glob
import os
import subprocess
import sys
import tempfile
import zipfile

# Project root = the directory that contains this tools/ folder, so it works from any cwd.
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JS = "pebble-js-app.js"
MAP = "pebble-js-app.js.map"


def run(cmd, **kw):
    return subprocess.run(cmd, cwd=ROOT, check=True, **kw)


def find_pbw():
    pbws = [p for p in glob.glob(os.path.join(ROOT, "build", "*.pbw"))
            if not p.endswith("-release.pbw")]
    if len(pbws) != 1:
        sys.exit("expected exactly one build/*.pbw, found: %s" % pbws)
    return pbws[0]


def minify(js_src):
    with tempfile.NamedTemporaryFile("wb", suffix=".js", delete=False) as tf:
        tf.write(js_src)
        tin = tf.name
    try:
        p = subprocess.run(["npx", "--yes", "terser", tin, "-c", "-m"],
                           stdout=subprocess.PIPE, cwd=ROOT)
    finally:
        os.unlink(tin)
    if p.returncode != 0 or not p.stdout:
        sys.exit("terser failed (returncode %s)" % p.returncode)
    js_min = p.stdout

    # Sanity check: the minified bundle must still parse.
    with tempfile.NamedTemporaryFile("wb", suffix=".js", delete=False) as tf:
        tf.write(js_min)
        tout = tf.name
    try:
        run(["node", "--check", tout])
    finally:
        os.unlink(tout)
    return js_min


def main():
    run(["pebble", "build"])
    src = find_pbw()
    out = src[:-len(".pbw")] + "-release.pbw"

    zin = zipfile.ZipFile(src)
    names = set(zin.namelist())
    js_src = zin.read(JS) if JS in names else None
    js_min = minify(js_src) if js_src is not None else None

    with zipfile.ZipFile(out, "w", zipfile.ZIP_STORED) as zout:
        for item in zin.infolist():
            if item.filename == MAP:
                continue  # drop the source map
            data = js_min if (item.filename == JS and js_min is not None) else zin.read(item.filename)
            zout.writestr(item.filename, data)
    zin.close()

    src_sz, out_sz = os.path.getsize(src), os.path.getsize(out)
    if js_src is not None:
        print("%s: %d -> %d bytes" % (JS, len(js_src), len(js_min)))
    else:
        print("no %s in the .pbw (nothing to minify); dropped source map only" % JS)
    print("pbw: %d -> %d bytes (%.0f%% smaller)" % (src_sz, out_sz, 100 * (1 - out_sz / src_sz)))
    print("wrote", out)


if __name__ == "__main__":
    main()
