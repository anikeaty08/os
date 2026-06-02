#!/bin/sh

set -eu

MARKER=${SMOKE_MARKER:-ASTRAOS_SMOKE_READY}
TIMEOUT=${SMOKE_TIMEOUT:-30}
ISO=${SMOKE_ISO:-astraos.iso}
DISK=${SMOKE_DISK:-disk.img}
SERIAL_LOG=${SMOKE_SERIAL_LOG:-smoke-serial.log}
QEMU_LOG=${SMOKE_QEMU_LOG:-smoke-qemu.log}
QEMU_PID=

cleanup() {
    if [ -n "$QEMU_PID" ] && kill -0 "$QEMU_PID" 2>/dev/null; then
        kill "$QEMU_PID" 2>/dev/null || true
        sleep 1
        if kill -0 "$QEMU_PID" 2>/dev/null; then
            kill -KILL "$QEMU_PID" 2>/dev/null || true
        fi
        wait "$QEMU_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT INT TERM

make clean
make iso
make disk

rm -f "$SERIAL_LOG" "$QEMU_LOG"

qemu-system-x86_64 \
    -cdrom "$ISO" \
    -drive "file=$DISK,format=raw,if=ide" \
    -m 256M \
    -serial "file:$SERIAL_LOG" \
    -display none \
    -no-reboot \
    -no-shutdown \
    >"$QEMU_LOG" 2>&1 &
QEMU_PID=$!

elapsed=0
while [ "$elapsed" -lt "$TIMEOUT" ]; do
    if [ -f "$SERIAL_LOG" ] && grep -q "$MARKER" "$SERIAL_LOG"; then
        echo "Smoke test passed: found $MARKER in $SERIAL_LOG"
        exit 0
    fi

    if ! kill -0 "$QEMU_PID" 2>/dev/null; then
        echo "Smoke test failed: QEMU exited before $MARKER appeared" >&2
        [ -f "$SERIAL_LOG" ] && tail -n 80 "$SERIAL_LOG" >&2
        [ -f "$QEMU_LOG" ] && tail -n 80 "$QEMU_LOG" >&2
        exit 1
    fi

    sleep 1
    elapsed=$((elapsed + 1))
done

echo "Smoke test failed: timed out after ${TIMEOUT}s waiting for $MARKER" >&2
[ -f "$SERIAL_LOG" ] && tail -n 80 "$SERIAL_LOG" >&2
[ -f "$QEMU_LOG" ] && tail -n 80 "$QEMU_LOG" >&2
exit 1
