#!/bin/bash
# Wrapper for qemu-pebble that adds:
#   -icount   emulated timers off the instruction counter, working around the
#             firmware timer-IRQ stalls seen under WSL2 (frozen clock after
#             1-5 min). Same as tools/qemu-icount-wrapper.sh elsewhere.
#   -qmp      a QMP socket, so touch can be injected into the running emulator
#             with tools/touch.py (the SDK emulator has no touch command).
#
# Use: PEBBLE_QEMU_PATH=<this script> pebble install --emulator emery
QMP_SOCK="${PEBBLE_QMP_SOCK:-/tmp/pb-qmp.sock}"
rm -f "$QMP_SOCK"
exec "$HOME/.pebble-sdk/SDKs/current/toolchain/bin/qemu-pebble" \
  -icount shift=auto,align=off,sleep=on \
  -qmp "unix:$QMP_SOCK,server=on,wait=off" \
  "$@"
