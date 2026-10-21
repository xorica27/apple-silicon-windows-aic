#!/usr/bin/env python3
"""Boot Linux on the QEMU apple-aic-virt machine."""

import argparse
import os
import select
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEFAULT_QEMU = Path("/tmp/qemu-m0-build/qemu-system-aarch64")
DEFAULT_KERNEL = Path("/tmp/linux-m2/linux-6.12/arch/arm64/boot/Image")
DEFAULT_INITRD = Path("/tmp/m2-initramfs.cpio.gz")
DEFAULT_DTB = Path("/tmp/apple-aic-virt.dtb")
DEFAULT_LOG = ROOT / "linux-boot.log"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", default=os.environ.get("QEMU", str(DEFAULT_QEMU)))
    parser.add_argument("--kernel", default=str(DEFAULT_KERNEL))
    parser.add_argument("--initrd", default=str(DEFAULT_INITRD))
    parser.add_argument("--dtb", default=str(DEFAULT_DTB))
    parser.add_argument("--smp", type=int, default=4)
    parser.add_argument("--memory", default="256M")
    parser.add_argument("--timeout", type=float, default=60.0)
    parser.add_argument("--log", default=str(DEFAULT_LOG))
    parser.add_argument("--append", default=(
        "console=ttyAMA0 earlycon=pl011,0x20010000 rdinit=/init panic=1"
    ))
    parser.add_argument("--trace-file", default="",
                        help="write AIC trace to this file")
    args = parser.parse_args()

    for label, path in (
        ("QEMU", args.qemu),
        ("kernel", args.kernel),
        ("initrd", args.initrd),
        ("dtb", args.dtb),
    ):
        if not Path(path).is_file():
            sys.stderr.write(f"{label} not found: {path}\n")
            return 2

    cmd = [
        args.qemu,
        "-M", "apple-aic-virt",
        "-smp", str(args.smp),
        "-m", args.memory,
        "-kernel", args.kernel,
        "-initrd", args.initrd,
        "-dtb", args.dtb,
        "-append", f"{args.append} m2.expected_cpus={args.smp}",
        "-nographic",
        "-no-reboot",
    ]

    if args.trace_file:
        cmd += ["-trace", f"enable=apple_aic_*,file={args.trace_file}"]

    print("running:", " ".join(cmd), flush=True)

    proc = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )

    output = []
    status = 1
    deadline = time.time() + args.timeout

    try:
        while time.time() < deadline:
            ready, _, _ = select.select([proc.stdout], [], [], 0.2)
            if not ready:
                if proc.poll() is not None:
                    break
                continue
            line = proc.stdout.readline()
            if not line:
                break
            output.append(line)
            sys.stdout.write(line)
            sys.stdout.flush()
            if "M2: PASS" in line:
                status = 0
                break
            if "Kernel panic" in line or "M2: FAIL" in line:
                status = 1
                break
        else:
            sys.stderr.write("timeout waiting for userspace/PASS\n")
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()

    log_path = Path(args.log)
    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_path.write_text("".join(output))

    print(f"\nlog: {log_path}")
    print("result:", "PASS" if status == 0 else "FAIL")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
