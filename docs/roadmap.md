# Roadmap

The roadmap is ordered by dependency, not ambition. Linux-on-AIC comes before
Windows; the bare-metal AIC payload comes before both.

## Goal and definition of done

**Goal:** boot Windows 11 ARM64 (pinned build) to a functional desktop on one
pinned Apple Silicon configuration, starting from a prepared image, with no
persistent EL2 hypervisor after handoff.

**Functional desktop:** all CPU cores online, timer and IPIs working, storage,
display, and keyboard/mouse usable.

**Targets:**

- Primary hardware: M1 Mac mini (`j274`, T8103, AIC v1).
- Secondary hardware: M1 MacBook Air (`j313`, T8103).
- Windows baseline: Windows 11 ARM64 24H2 (build 26100); 25H2 secondary.
- Model: `apple_aic` from the pinned `jprx/qemu-sptm` checkout.

**Not success:** reaching a setup screen, a kernel handoff, or a boot under an
EL2 shim. The threshold is a desktop with SMP, timer, storage, display, and
input, plus the no-EL2 verification protocol.

## M0 — Observability and oracle

**Goal:** make the existing model measurable and compare it with real hardware.

Tasks:

- Replace the `aic_log` no-op with QEMU trace events for read, write, raise, and
  EOI.
- Add a qtest for the AIC register map.
- Write a bare-metal AIC payload: enable a timer, enable a UART IRQ, perform
  EOI, then send an IPI.
- Run the payload on the QEMU model and on `m1n1` hardware.
- Record the EL2 state before and after handoff for the baseline.

Exit criteria:

- A register trace from the existing `darwin` machine.
- The payload passes on both model and hardware, or the differences are
  documented.
- The no-EL2 verification harness has a defined output format.

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
- Model-versus-hardware parity is recorded.

Dependencies: M2.  
Main risk: model passes simple tests but fails under load.

## M4 — Stock Windows on AIC

**Goal:** reproduce the known failure on the new machine.

Tasks:

- Add UEFI (EDK2) and ACPI tables.
- Add NVMe, xHCI, USB keyboard/storage, and ramfb.
- Boot a stock Windows 11 ARM64 24H2 image.
- Capture the `ntoskrnl` freeze and the AIC access trace.

Exit criteria:

- A reproducible freeze at interrupt-controller initialization, with a trace.
- The failure is documented as the baseline for the intervention.

Dependencies: M2 (M3 recommended).  
Main risk: ACPI bring-up consumes more time than the AIC work.

## M5 — Windows intervention

**Goal:** boot Windows without the EL2 shim on the model.

Tasks:

- Compare the GIC-shaped adapter and the HAL patch; choose the smaller seam.
- Patch or redirect the HAL's GIC initialization, ack/EOI, mask/unmask, target,
  and IPI paths to AIC operations.
- Bring up the timer first, then one device IRQ, then IPIs/SMP, then storage,
  display, and input.
- Document every patched site and its Windows build.
- Run the no-EL2 verification protocol.

Exit criteria:

- Windows reaches a functional desktop on the AIC model.
- The intervention surface and failure modes are documented.
- The no-EL2 protocol passes on the model, or the failure is reported honestly.

Dependencies: M4.  
Main risk: the HAL has no clean seam; the patch becomes broad and fragile.

## M6 — Hardware validation

**Goal:** verify the model against a real M1.

Tasks:

- Use a sacrificial M1 Mac mini with `m1n1` serial and a second host.
- Boot the same image on hardware.
- Compare AIC traces, timer behavior, and interrupt latency.
- Run the no-EL2 verification protocol on hardware.
- Document hardware-only quirks and errata.

Exit criteria:

- A functional desktop on hardware, with the no-EL2 protocol passing, or the
  blocking differences are documented.

Dependencies: M5.  
Main risk: data loss, boot-chain changes, model/hardware divergence.

## No-EL2 verification protocol

1. Before handoff, record the EL2 state and confirm the bootloader will drop to
   EL1.
2. After `ExitBootServices`, confirm no EL2 vector table is active and no
   stage-2 translation is enabled.
3. Confirm no vGIC or PSCI emulation is running; the OS programs AIC directly.
4. A Windows test driver reads `CurrentEL` and reports EL1, combined with an
   external firmware/`m1n1` check and the absence of EL2 code.
5. Record register dumps, trace logs, and the driver report per hardware run.

If the protocol cannot be satisfied, the result is "EL2 remains required," not a
bare-metal success.

## Minimum publishable result

- A faithful, documented AIC model and general-purpose AIC machine.
- A reproducible Linux-on-AIC test bench with SMP/timer/IPI results.
- A characterization of the Windows HAL's GIC assumptions and the minimal seam.
- A positive or negative answer on hypervisor-free feasibility.

The stretch result is a patched Windows desktop on the model, then on hardware,
with the no-EL2 protocol passing.

## First patch scope

The smallest useful change set is M0 plus the first half of M1:

1. Trace events.
2. `num-cpus` and per-CPU IRQ lines.
3. IPI set/clear and target registers.
4. A qtest and a bare-metal payload.
5. A minimal `apple-aic-virt` machine that boots a Linux device tree.

That is enough to produce a credible AIC model before investing in UEFI, ACPI,
and the Windows intervention.

## Decision points

- **After M3:** is the model faithful enough to justify Windows work?
- **After M4:** is there a viable seam — adapter or patch — or does the
  intervention require broad changes?
- **After M5:** is the result maintainable, or is a minimal EL2 layer the
  permanent answer without vendor support?
- **After M6:** does the no-EL2 protocol pass on hardware; if not, what exactly
  remains at EL2?
