#!/usr/bin/env bash
# Apply the M0 observability patch to the pinned qemu-sptm checkout.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QEMU="$ROOT/upstream/darwin-vm/qemu-sptm"
PATCH="$ROOT/patches/0001-m0-observability.patch"

if [ ! -d "$QEMU/.git" ]; then
    printf 'missing upstream checkout; run scripts/fetch-upstream.sh first\n' >&2
    exit 1
fi

if [ ! -f "$PATCH" ]; then
    printf 'missing patch: %s\n' "$PATCH" >&2
    exit 1
fi

if git -C "$QEMU" apply --reverse --check "$PATCH" >/dev/null 2>&1; then
    printf 'patch already applied\n'
    exit 0
fi

git -C "$QEMU" apply --check "$PATCH"
git -C "$QEMU" apply "$PATCH"
printf 'applied %s\n' "$PATCH"
