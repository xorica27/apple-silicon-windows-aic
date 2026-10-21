# Bare-Metal Windows 11 on Apple Silicon: The AIC Gate

**Research concept — not a product, not an installer.**

## Goal

Boot Windows 11 ARM64 (pinned build) to a **functional desktop** on one pinned
Apple Silicon configuration, starting from a prepared image, with **no
persistent EL2 hypervisor after handoff**.

"Functional desktop" means: all CPU cores online, timer and IPIs working,
storage, display, and keyboard/mouse usable. Installer/OOBE, GPU acceleration,
and production support are out of scope.

## What "native" means here

"Native" is overloaded. This project uses a three-level distinction:

| Mode | What runs beneath Windows | In scope? |
|---|---|---|
| **VM** (Parallels, UTM, VMware) | macOS + a hypervisor | No |
| **Bare metal with EL2 shim** | `m1n1` at EL2 presenting a virtual GIC/PSCI | Baseline, not the goal |
| **Hypervisor-free bare metal** | Firmware/bootloader only; hands off at EL1 | **Goal** |

A bootloader and firmware may still run before Windows. What must be absent is a
**persistent hypervisor at EL2** after handoff: no EL2 code, no stage-2
translation, no virtual GIC, no PSCI emulation. "Native" here does **not** mean
"supported, licensed, or endorsed by Apple or Microsoft."

## The gate

Windows' ARM64 HAL initializes the root interrupt controller before any normal
driver can load, and expects a GIC Distributor in the ACPI MADT. Apple Silicon
exposes the proprietary AIC. The historical HAL-extension route for interrupt
controllers was removed before Windows 8 RTM. Current community projects boot
Windows only by running an EL2 hypervisor that presents a virtual GIC and PSCI.

This repository studies whether that layer can be removed — and if not, exactly
what is missing. The AIC is the **gate**; storage, display, input, power, and
GPU drivers are the larger work behind it.

## Primary targets

- **Hardware:** `MacBookPro18,1` (Apple M1 Pro, t600x, AIC v2) as the available
  primary test machine; M1 Mac mini (`j274`, T8103, AIC v1) and M1 MacBook Air
  (`j313`, AIC v1) as secondary targets. M2+ AIC variants are a separate support
  matrix.
- **Windows:** Windows 11 ARM64 24H2 (build 26100) as the baseline; 25H2 as a
  secondary target. The patch is build-specific; every experiment records the
  build number.
- **Model:** the `apple_aic` device model in the pinned
  `jprx/qemu-sptm` checkout.

## Minimum publishable result

Even if Windows never reaches a desktop, the project can publish:

- a faithful, documented AIC model and a general-purpose AIC machine;
- a reproducible Linux-on-AIC test bench;
- a characterization of the Windows HAL's GIC assumptions and the minimal seam;
- a positive or negative answer on hypervisor-free feasibility.

The stretch result is a patched Windows desktop on the model, then on hardware.

## Status

Concept and upstream survey (October 2026). There is no Windows patch here.
Milestone 0 (QEMU AIC observability bench) is concluded for AIC v1 and v2.
Milestone 0b (M1 Pro hardware execution) is open: the hardware payload is
cross-compiled but has not been booted. Milestone 1's model portion is complete
on QEMU: multi-CPU, targeting, AICv1 MMIO IPIs, mask/SW/hardware-state
registers, 64-bit MMIO, and reset; qtest 15/15; the SMP/IPI payload passes. The
Linux AIC driver probe moves to M2.

## Documents

- [Research concept](docs/research-concept.md)
- [Prior art](docs/prior-art.md)
- [Upstream survey](docs/upstream-survey.md)
- [Roadmap](docs/roadmap.md)
- [Milestone 0 / 0b bench](m0/README.md)
- [Milestone 1 model work](m1/README.md)
- [Upstream source inspection, 2026-10-21](notes/2026-10-21-upstream-inspection.md)
- [M1 Pro live hardware inspection, 2026-10-21](notes/2026-10-21-m1-pro-live-hardware.md)

## Repository layout

```text
m0/         Milestone 0/0b bench: QEMU machine patch, qtest, payload, trace
m1/         Milestone 1 model work: SMP/IPI results and trace sample
patches/    QEMU changes as patches
docs/       research concept, prior art, upstream survey, roadmap
notes/      dated lab and inspection notes
scripts/    fetch-upstream.sh, apply-m0-patches.sh
upstream/   cloned upstream sources (gitignored)
```

## Quick start

```bash
./scripts/fetch-upstream.sh
```

This shallow-clones `jprx/darwin-vm` and its `qemu-sptm` submodule, pinned in the
[inspection note](notes/2026-10-21-upstream-inspection.md). The upstream QEMU
fork is GPL-2.0 and is deliberately not vendored here.

## Warnings

- Research only. Booting experiments can destroy disks and macOS installs.
- No Microsoft or Apple binaries are included or distributed.
- Nothing here is supported, authorized, or endorsed by Apple or Microsoft.
- Upstream QEMU and Linux AIC code is GPL-2.0. Keep it separate from any
  proprietary Windows patch.

## License

Original content is MIT-licensed. Upstream components retain their own licenses.
See [LICENSE](LICENSE).
