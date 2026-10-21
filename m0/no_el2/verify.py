#!/usr/bin/env python3
"""Validate a no-persistent-EL2 verification report.

Exit codes: 0 = pass, 1 = fail, 2 = pending/invalid.
"""

import argparse
import json
import sys
from pathlib import Path

SECTION_KEYS = ["status", "run", "handoff", "checks", "evidence"]
RUN_KEYS = ["id", "date", "hardware", "windows_build", "bootloader"]
CHECK_KEYS = [
    "el2_vector_active",
    "stage2_enabled",
    "vgic_present",
    "psci_emulation",
    "aic_direct",
    "current_el_windows",
]


def evaluate(report: dict) -> tuple[str, list[str]]:
    missing = [key for key in SECTION_KEYS if key not in report]
    if missing:
        return "invalid", [f"missing section: {key}" for key in missing]

    if report["status"] == "pending":
        return "pending", ["report is marked pending"]

    run = report["run"]
    missing = [key for key in RUN_KEYS if key not in run]
    if missing:
        return "invalid", [f"missing run field: {key}" for key in missing]

    handoff = report["handoff"]
    checks = report["checks"]

    if handoff.get("el_at_handoff") is None:
        return "pending", ["handoff.el_at_handoff is null"]
    if handoff.get("el2_code_after_exit_boot_services") is None:
        return "pending", ["handoff.el2_code_after_exit_boot_services is null"]

    for key in CHECK_KEYS:
        if key not in checks or checks[key] is None:
            return "pending", [f"checks.{key} is null"]

    reasons = []

    if handoff["el_at_handoff"] != 1:
        reasons.append(
            f"OS handoff at EL{handoff['el_at_handoff']}, expected EL1"
        )
    if handoff["el2_code_after_exit_boot_services"]:
        reasons.append("EL2 code runs after ExitBootServices")
    if checks["el2_vector_active"]:
        reasons.append("EL2 vector table is active")
    if checks["stage2_enabled"]:
        reasons.append("stage-2 translation is enabled")
    if checks["vgic_present"]:
        reasons.append("a virtual GIC is present")
    if checks["psci_emulation"]:
        reasons.append("PSCI is being emulated")
    if checks["aic_direct"] is not True:
        reasons.append("AIC is not driven directly")
    if checks["current_el_windows"] != 1:
        reasons.append(
            f"Windows reports EL{checks['current_el_windows']}, expected EL1"
        )

    if reasons:
        return "fail", reasons
    return "pass", []


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("report", type=Path)
    parser.add_argument("--json", action="store_true", dest="as_json")
    args = parser.parse_args()

    try:
        report = json.loads(args.report.read_text())
    except (OSError, json.JSONDecodeError) as exc:
        print(f"invalid report: {exc}", file=sys.stderr)
        return 2

    status, reasons = evaluate(report)
    result = {
        "report": str(args.report),
        "status": status,
        "reasons": reasons,
    }

    if args.as_json:
        print(json.dumps(result, indent=2))
    else:
        print(f"status: {status}")
        for reason in reasons:
            print(f"- {reason}")

    if status == "pass":
        return 0
    if status == "pending":
        return 2
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
