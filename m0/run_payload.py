#!/usr/bin/env python3
"""Run the AIC payload on the QEMU apple-aic test machines."""

import argparse
import os
import select
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
DEFAULT_QEMU = Path("/tmp/qemu-m0-build/qemu-system-aarch64")
DEFAULT_LOG = ROOT / "trace" / "payload-run.log"

BOARDS = {
    "v1": ("apple-aic-test", "payload-v1.elf", 2),
    "v2": ("apple-aic-v2-test", "payload-v2.elf", 1),
}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--board", choices=sorted(BOARDS), default="v1")
    parser.add_argument("--qemu", default=os.environ.get("QEMU", str(DEFAULT_QEMU)))
    parser.add_argument("--payload", default="")
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--log", default=str(DEFAULT_LOG))
    parser.add_argument("--trace", action="store_true",
                        help="enable AIC trace (interleaves with serial output)")
    parser.add_argument("--trace-file", default="",
                        help="write AIC trace to this file instead of stderr")
    args = parser.parse_args()

    machine, default_payload, smp = BOARDS[args.board]
    payload = args.payload or str(ROOT / "payload" / default_payload)

    if not Path(args.qemu).is_file():
        sys.stderr.write(f"QEMU not found: {args.qemu}\n")
        return 2
    if not Path(payload).is_file():
        sys.stderr.write(f"payload not found: {payload}\n")
        return 2

    cmd = [
        args.qemu,
        "-M", machine,
        "-smp", str(smp),
        "-m", "128M",
        "-nographic",
        "-device", f"loader,file={payload},cpu-num=0",
    ]

    if args.trace_file:
        cmd += ["-trace", f"enable=apple_aic_*,file={args.trace_file}"]
    elif args.trace:
        cmd += ["-trace", "enable=apple_aic_*"]

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
                continue
            line = proc.stdout.readline()
            if not line:
                break
            output.append(line)
            sys.stdout.write(line)
            sys.stdout.flush()
            if "PASS" in line:
                status = 0
                break
            if "FAIL" in line or "EXCEPTION" in line:
                status = 1
                break
        else:
            sys.stderr.write("timeout waiting for PASS/FAIL\n")
    finally:
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
