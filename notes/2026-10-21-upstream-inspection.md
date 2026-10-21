# Upstream inspection — 2026-10-21

## Checkout

- `upstream/darwin-vm` @ `edfc19433da8d04d5927a7b1932acce4034f307c` (2026-10-05)
  - MIT wrapper; ~1,230 stars; iOS/macOS in QEMU.
- `upstream/darwin-vm/qemu-sptm` @ `bad6336fb24211f8c4e8e946c259bd5b12417340` (2026-09-26)
  - QEMU 11.1.0 fork; GPL-2.0.
- Both are shallow clones. The recursive submodule update also pulled QEMU's
  `roms/*` submodules (edk2, openssl, etc.), making the checkout ~2.0 GB.
  `scripts/fetch-upstream.sh` reproduces only the two repositories needed to
  read the source.

## `hw/intc/apple_aic.c` — what exists

The file's own header says it best (lines 9–17):

> minimal aic implementation: knows how many interrupts there are; knows which
> are enabled/disabled; can receive interrupts from devices, and send irqs to
> core 0; can respond to eoi conditions; supports v1-3; ignores everything else.

Concrete capabilities:

- **AIC v1/v2/v3 register layouts** (lines 36–56):
  - v1: num_ints `0x4`, IACK `0x2004`, ack `0x4080`, disable `0x4100`,
    enable `0x4180`.
  - v2: num_ints `0x0C`, disable `0x6400`, enable `0x6600`.
  - v3: num_ints `0x0C`, disable `0x14400`, enable `0x14600`.
- **Interrupt count**:
  - v1 derives it from the device-tree `ipid-mask` length (`8 * len`).
  - v2/v3 use a hard-coded `A_GOOD_NUMBER_OF_IRQS = 0x1000` (line 31).
- **Enable/disable** bit-vectors (lines 145–157), **pending** state (lines
  171–176), and **EOI/IACK** (lines 109–124, 159–166).
- **One output IRQ line**, raised while any pending+enabled interrupt exists
  (lines 99–107). It is wired to the CPU's IRQ in `darwin.c` (line 287).
- **Unknown MMIO reads return 0** and unknown writes are ignored (lines 126–169).
- **32-bit MMIO only**: `assert(sizeof(uint32_t) == size)` (lines 129, 141).

What it does **not** implement:

- SMP or per-CPU interrupt targeting — `qemu_get_cpu(0)` in the log and a
  single `irq_out`; the machine sets `max_cpus = 1`.
- FIQ delivery. In `darwin.c`, the platform timer (GTIMER_HYPVIRT) is wired
  directly to the CPU FIQ, separately from AIC (lines 284–287).
- IPIs/SGIs.
- Priorities, affinities, active/pending semantics beyond a boolean.
- Most other AIC registers.
- 64-bit MMIO.

The source is explicit that this is enough for a root shell: a working AIC is
only needed for the secondary "qemuport" UARTs; UART0 does not use interrupts
(lines 265–274).

## `hw/arm/darwin.c` — the machine around it

- Machine name `darwin`, description "Generic Apple Silicon Device"
  (lines 300–305).
- **Single CPU**: `max_cpus = 1` (line 303). This is the biggest limitation for
  Windows, which needs SMP.
- **EL2 enabled, EL3 disabled** (lines 222–232); XNU can run at EL1 or EL2, SPTM
  requires EL2.
- Boots a Darwin **kernelcache** plus device tree, trust cache, ramdisk, and
  optional SPTM/TXM — not UEFI, not ACPI, not a Windows boot manager
  (lines 198–216).
- Finds `arm-io/aic` in the device tree and maps its MMIO using the `arm-io`
  `ranges` base (lines 250–255).
- UART0 is polled; the four `qemuport` UARTs take AIC IRQs (lines 257–273).
- Wires `GTIMER_HYPVIRT` → CPU FIQ and AIC output → CPU IRQ (lines 284–287).
  This mirrors Apple's hardwired FIQ-to-timer arrangement.
- No PCIe, NVMe, USB, display, DART, PMGR, or SMC models.

## `hw/arm/apple_dtree.c` — device-tree parser

- Read-only ADT parser; deliberately cannot create or resize nodes (lines
  10–16).
- Assumes the tree was preprocessed by `dt_fixup` in the parent repo (line 15).
- Strips the `BIT(31)` flag some IPSW length fields carry (lines 22–26).
- Path lookup (`adt_find_node`) walks `name` properties (lines 71–100).

## What is reusable vs. what must be built

Reusable directly:

- The AIC v1/v2/v3 register offsets and enable/disable/EOI semantics.
- The ADT parser and the pattern of locating `arm-io/aic` + `ranges`.
- The machine wiring pattern (AIC → CPU IRQ, timer → CPU FIQ).
- The `dt_fixup` approach for turning an IPSW device tree into something QEMU
  can parse.

Must be extended or built:

1. **SMP**: multiple CPU objects, per-CPU IRQ/FIQ lines, AIC target/affinity
   handling, CPU startup. The current model cannot exercise any of this.
2. **IPIs** and **FIQ** routing; Windows will need both paths modeled.
3. **Timer**: the current machine hardwires the timer to FIQ and does not route
   it through AIC. Windows expects a timer interrupt through the platform
   interrupt controller.
4. **More AIC registers** so Windows' HAL does not read unexpected zeros.
5. **A Windows-capable machine**: UEFI (EDK2), ACPI tables, boot manager,
   storage, display, input. The `darwin` machine boots a kernelcache, not a
   UEFI payload.
6. **The Windows HAL patch itself** — still absent from every public project.

## Licensing

- `qemu-sptm` is GPL-2.0. `hw/intc/apple_aic.c` is part of that GPL codebase.
- `darwin-vm`'s wrapper is MIT, but its QEMU submodule is not.
- Linux `irq-apple-aic.c` and `m1n1` are also GPL-2.0.
- Keep the QEMU bench separate from any proprietary Windows patch, or
  reimplement the semantics clean-room.

## Next experiments

1. Enable `aic_log` (line 24–25) and boot the `darwin` machine; capture the
   exact register access sequence XNU performs.
2. Write a bare-metal AIC test payload that exercises enable/disable, EOI, and
   a second interrupt source, and run it on the model and on real `m1n1`
   hardware.
3. Add per-CPU IRQ lines and an IPI register path, then test SMP with a small
   bare-metal payload before involving Windows.
4. Add a timer path through AIC and compare with the FIQ wiring.
5. Only then build the EDK2/ACPI machine and attempt a Windows boot with the
   HAL patch.
