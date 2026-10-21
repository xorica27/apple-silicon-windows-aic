# Upstream survey: the existing QEMU AIC model

This document records what the pinned upstream checkout provides, what can be
reused, and what must be built. It complements the dated
[source inspection](../notes/2026-10-21-upstream-inspection.md).

## Pinned sources

| Repository | Commit | Date | License | Role |
|---|---|---|---|---|
| [`jprx/darwin-vm`](https://github.com/jprx/darwin-vm) | `edfc19433da8d04d5927a7b1932acce4034f307c` | 2026-10-05 | MIT wrapper; QEMU submodule GPL-2.0 | Harness, firmware scripts, device-tree fixup |
| [`jprx/qemu-sptm`](https://github.com/jprx/qemu-sptm) | `bad6336fb24211f8c4e8e946c259bd5b12417340` | 2026-09-26 | GPL-2.0 | QEMU 11.1.0 fork with Apple Silicon device models |

The upstream checkout is intentionally not vendored. Run
`./scripts/fetch-upstream.sh` for a slim clone of the two repositories. A
recursive submodule update also pulls QEMU's `roms/*` trees and grows the
checkout to about 2 GB; that is unnecessary for reading the AIC model.

## `hw/intc/apple_aic.c`

The file describes itself as a minimal implementation: it knows the interrupt
count, enable/disable state, pending state, and EOI, sends IRQs to core 0, and
"ignores everything else" (lines 9–17).

Implemented:

- **AIC v1/v2/v3 register layouts** (lines 36–56).
- **Interrupt count**: v1 from the device-tree `ipid-mask` length; v2/v3 from a
  hard-coded `0x1000`.
- **Enable/disable** bit-vectors (lines 145–157).
- **Pending** state (lines 171–176).
- **EOI/IACK** (lines 109–124, 159–166).
- **One IRQ output line**, raised while any pending+enabled interrupt exists
  (lines 99–107).
- **Unknown reads return 0; unknown writes are ignored** (lines 126–169).
- **32-bit MMIO only** (lines 129, 141).

Not implemented:

- SMP or per-CPU targeting; the model logs `qemu_get_cpu(0)` and has one
  `irq_out`.
- FIQ delivery. `darwin.c` wires the platform timer directly to CPU FIQ,
  separately from AIC (lines 284–287).
- IPIs/SGIs.
- Priorities, affinities, or active/pending state beyond a boolean.
- Most other AIC registers.
- 64-bit MMIO.

The source is explicit that this is enough for a root shell: a working AIC is
only needed for the secondary "qemuport" UARTs; UART0 is polled.

## `hw/arm/darwin.c`

- Machine `darwin`, "Generic Apple Silicon Device" (lines 300–305).
- **Single CPU**: `max_cpus = 1` (line 303).
- EL2 enabled, EL3 disabled (lines 222–232).
- Boots a Darwin **kernelcache** with device tree, trust cache, ramdisk, and
  optional SPTM/TXM (lines 198–216). There is no UEFI, ACPI, or Windows boot
  path.
- Finds `arm-io/aic` in the device tree and maps it using the `arm-io` `ranges`
  base (lines 250–255).
- UART0 is polled; four `qemuport` UARTs use AIC interrupts (lines 257–273).
- Wires `GTIMER_HYPVIRT` to CPU FIQ and AIC output to CPU IRQ (lines 284–287).

## `hw/arm/apple_dtree.c`

- Read-only ADT parser; it cannot create or resize nodes (lines 10–16).
- Assumes the tree was preprocessed by `dt_fixup` in the parent repository.
- Strips the `BIT(31)` flag some IPSW length fields carry (lines 22–26).
- Path lookup walks `name` properties (lines 71–100).

## Reusable pieces

- AIC v1/v2/v3 register offsets and enable/disable/EOI semantics.
- The ADT parser and the `arm-io/aic` + `ranges` mapping pattern.
- The machine wiring pattern: AIC to CPU IRQ, timer to CPU FIQ.
- The `dt_fixup` approach for converting an IPSW device tree into a QEMU-parseable
  tree.

## Gaps to close

1. **SMP**: multiple CPUs, per-CPU IRQ/FIQ outputs, target/affinity handling,
   CPU startup.
2. **IPIs and FIQ routing**.
3. **Timer through AIC**: the current machine hardwires the timer to FIQ and
   does not route it through AIC.
4. **More AIC registers** so a general OS does not read unexpected zeros.
5. **A general-purpose machine**: UEFI/ACPI, storage, display, input, and
   Windows-capable devices.
6. **The Windows HAL patch**, which no public project provides.

## Suggested first change set

1. Add QEMU `trace-events` for AIC reads, writes, raises, and EOI.
2. Add `num-cpus` and per-CPU IRQ lines to `AppleAICState`.
3. Add IPI set/clear and target registers.
4. Add a qtest and a bare-metal AIC payload.
5. Build a minimal `apple-aic-virt` machine that boots a Linux device tree.

## Licensing

`qemu-sptm` and the Linux AIC driver are GPL-2.0. `darwin-vm`'s wrapper is MIT,
but its QEMU submodule is not. Keep the QEMU bench separate from any proprietary
Windows patch, or reimplement the semantics clean-room.
