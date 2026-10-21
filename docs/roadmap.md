# Roadmap

The roadmap is ordered by dependency, not ambition. Linux-on-AIC comes before
Windows; the bare-metal AIC payload comes before both.

## M0 — Observability and oracle

**Goal:** make the existing model measurable and compare it with real hardware.

Tasks:

- Replace the `aic_log` no-op with QEMU trace events for read, write, raise, and
  EOI.
- Add a qtest for the AIC register map.
- Write a bare-metal AIC payload: enable a timer, enable a UART IRQ, perform
  EOI, then send an IPI.
- Run the payload on the QEMU model and on `m1n1` hardware.

Exit criteria:

- A register trace from the existing `darwin` machine.
- The payload passes on both model and hardware, or the differences are
  documented.

Dependencies: none.  
Main risk: treating model behavior as hardware truth.

## M1 — Extend the AIC model

**Goal:** a multi-CPU AIC model that a general OS can drive.

Tasks:

- Add `num-cpus`; per-CPU IRQ and FIQ outputs; per-IRQ target CPU.
- Add IPI set/clear handling.
- Add mask set/clear, hardware-state, and info registers.
- Derive the real interrupt count from the ADT/property instead of hard-coding
  `0x1000`.
- Add 64-bit MMIO where the controller allows it.
- Add reset behavior.

Exit criteria:

- Bare-metal SMP and IPI tests pass in QEMU.
- The Linux AIC driver probes successfully on the model.

Dependencies: M0.  
Main risk: AIC semantics that Linux uses but the model omits.

## M2 — General-purpose machine

**Goal:** an `apple-aic-virt` machine that boots Linux.

Tasks:

- Model the machine on QEMU `virt`, but replace GIC with AIC.
- Add RAM, 1–8 CPUs, PL011 UART, RTC, and `fw_cfg`.
- Add `virtio-mmio` or PCIe with `virtio-blk` and `virtio-net`.
- Wire each CPU's IRQ/FIQ to AIC.
- Provide a device tree describing AIC, timer, UART, and virtio.

Exit criteria:

- Linux boots to userspace with `console=ttyAMA0`.

Dependencies: M1.  
Main risk: QEMU `virt` assumptions leaking through ACPI or GIC helpers.

## M3 — Linux validation

**Goal:** prove the model is faithful enough to trust.

Tasks:

- Boot with 4 CPUs; confirm all cores online.
- Exercise timer, IPIs, and IRQ affinity.
- Run `cyclictest`, an interrupt storm, and a virtio-blk soak.
- Compare behavior with `m1n1` hardware where possible.

Exit criteria:

- No lost or stuck interrupts under stress.
- Timer drift and interrupt latency are measured and documented.

Dependencies: M2.  
Main risk: model passes simple tests but fails under load.

## M4 — Stock Windows on AIC

**Goal:** reproduce the known failure on the new machine.

Tasks:

- Add UEFI (EDK2) and ACPI tables.
- Add NVMe, xHCI, USB keyboard/storage, and ramfb.
- Boot a stock Windows 11 ARM64 image.
- Capture the `ntoskrnl` freeze and the AIC access trace.

Exit criteria:

- A reproducible freeze at interrupt-controller initialization, with a trace.

Dependencies: M2 (M3 recommended).  
Main risk: ACPI bring-up consumes more time than the AIC work.

## M5 — Windows HAL patch

**Goal:** boot Windows without the EL2 shim on the model.

Tasks:

- Choose the patch level (offline image first).
- Patch the HAL's GIC initialization, ack/EOI, mask/unmask, target, and IPI
  paths to call AIC operations.
- Bring up the timer first, then one device IRQ, then IPIs/SMP, then storage.
- Document every patched site and its Windows build.

Exit criteria:

- Windows reaches a desktop on the AIC model.
- The patch surface and failure modes are documented.

Dependencies: M4.  
Main risk: the HAL has no clean seam; the patch becomes broad and fragile.

## M6 — Hardware validation

**Goal:** verify the model against a real M1.

Tasks:

- Use a sacrificial M1 with `m1n1` serial and a second host.
- Boot the same image on hardware.
- Compare AIC traces, timer behavior, and interrupt latency.
- Document hardware-only quirks and errata.

Exit criteria:

- The image boots on hardware, or the blocking differences are documented.

Dependencies: M5.  
Main risk: data loss, boot-chain changes, model/hardware divergence.

## First patch scope

The smallest useful change set is M0 plus the first half of M1:

1. Trace events.
2. `num-cpus` and per-CPU IRQ lines.
3. IPI set/clear and target registers.
4. A qtest and a bare-metal payload.
5. A minimal `apple-aic-virt` machine that boots a Linux device tree.

That is enough to produce a credible AIC model before investing in UEFI, ACPI,
and the Windows HAL patch.

## Decision points

- **After M3:** is the model faithful enough to justify Windows work?
- **After M4:** is there a viable HAL seam, or does the patch require broad
  changes?
- **After M5:** is the result maintainable, or is a minimal EL2 layer the
  permanent answer without vendor support?
