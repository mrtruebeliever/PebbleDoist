#!/usr/bin/env python
"""Inject touch into the emery emulator over QEMU's QMP socket.

The SDK emulator has no touch command (`pebble emu-button` covers buttons
only), but qemu-pebble does emulate the PT2 touchscreen, so touch can be fed in
through QEMU's absolute-pointer input path -- the same route the firmware
repo's own `./pbl touch` uses.

Needs the emulator started with tools/qemu-touch-wrapper.sh, which opens the
QMP socket:

    export PEBBLE_QEMU_PATH=$PWD/tools/qemu-touch-wrapper.sh
    pebble install --emulator emery

Usage:
    tools/touch.py tap 100 120
    tools/touch.py swipe 100 200 100 60 [steps] [duration_s]
"""
import json
import os
import socket
import sys
import time

# Emery/PT2 panel, and QEMU's fixed absolute-axis range (0..32767).
SCREEN_W, SCREEN_H = 200, 228
ABS_MAX = 32767

SOCK = os.environ.get("PEBBLE_QMP_SOCK", "/tmp/pb-qmp.sock")


class Qmp(object):
    def __init__(self, path=SOCK):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(5)
        self.sock.connect(path)
        self.buf = b""
        self._read()                      # greeting
        self.cmd("qmp_capabilities")

    def _read(self):
        while b"\n" not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError("QMP socket closed")
            self.buf += chunk
        line, self.buf = self.buf.split(b"\n", 1)
        return json.loads(line)

    def cmd(self, name, **args):
        payload = {"execute": name}
        if args:
            payload["arguments"] = args
        self.sock.sendall((json.dumps(payload) + "\n").encode())
        while True:
            msg = self._read()
            if "event" in msg:            # async events interleave with replies
                continue
            if "error" in msg:
                raise RuntimeError(msg["error"])
            return msg

    def close(self):
        self.sock.close()


def scale(x, y):
    return (int(x * ABS_MAX / (SCREEN_W - 1)), int(y * ABS_MAX / (SCREEN_H - 1)))


def move(qmp, x, y, button=None):
    ax, ay = scale(x, y)
    events = [
        {"type": "abs", "data": {"axis": "x", "value": ax}},
        {"type": "abs", "data": {"axis": "y", "value": ay}},
    ]
    if button is not None:
        events.append({"type": "btn", "data": {"down": button, "button": "left"}})
    qmp.cmd("input-send-event", events=events)


def tap(qmp, x, y, hold=0.08):
    move(qmp, x, y, button=True)
    time.sleep(hold)
    move(qmp, x, y, button=False)


def swipe(qmp, x1, y1, x2, y2, steps=12, duration=0.35):
    move(qmp, x1, y1, button=True)
    for i in range(1, steps + 1):
        move(qmp, x1 + (x2 - x1) * i // steps, y1 + (y2 - y1) * i // steps)
        time.sleep(duration / steps)
    move(qmp, x2, y2, button=False)


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 1
    what = sys.argv[1]
    qmp = Qmp()
    try:
        if what == "tap":
            tap(qmp, int(sys.argv[2]), int(sys.argv[3]))
        elif what == "swipe":
            args = [int(a) for a in sys.argv[2:6]]
            steps = int(sys.argv[6]) if len(sys.argv) > 6 else 12
            duration = float(sys.argv[7]) if len(sys.argv) > 7 else 0.35
            swipe(qmp, *args, steps=steps, duration=duration)
        else:
            print(__doc__)
            return 1
    finally:
        qmp.close()
    print("ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
