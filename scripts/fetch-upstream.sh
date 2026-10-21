#!/usr/bin/env bash
# Fetch only the repositories needed to read the AIC device model.
# This deliberately skips QEMU's recursive roms/* submodules.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="$ROOT/upstream/darwin-vm"

if [ ! -d "$DEST/.git" ]; then
    git clone --depth 1 https://github.com/jprx/darwin-vm.git "$DEST"
fi

git -C "$DEST" submodule update --init --depth 1 qemu-sptm

printf 'darwin-vm: %s\n' "$(git -C "$DEST" rev-parse HEAD)"
printf 'qemu-sptm: %s\n' "$(git -C "$DEST/qemu-sptm" rev-parse HEAD)"
