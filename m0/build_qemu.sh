#!/usr/bin/env bash
# Build the QEMU fork in a path without spaces (QEMU configure rejects them).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="$ROOT/upstream/darwin-vm/qemu-sptm"
WORK="${QEMU_WORK:-/tmp/qemu-sptm-m0}"
BUILD="${QEMU_BUILD:-/tmp/qemu-m0-build}"

if [ ! -d "$SRC/.git" ]; then
    printf 'missing upstream checkout; run scripts/fetch-upstream.sh first\n' >&2
    exit 1
fi

rsync -a --exclude='/.git' --exclude='roms/' "$SRC/" "$WORK/"
mkdir -p "$BUILD"

cd "$BUILD"
"$WORK/configure" --target-list=aarch64-softmmu --disable-pvg
ninja -j2 qemu-system-aarch64 tests/qtest/apple-aic-test

printf '\nbuilt: %s/qemu-system-aarch64\n' "$BUILD"
printf 'qtest: %s/tests/qtest/apple-aic-test\n' "$BUILD"
