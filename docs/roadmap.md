# Roadmap

The roadmap is ordered by dependency, not ambition. Linux-on-AIC comes before
Windows; the bare-metal AIC payload comes before both.

## Status (2026-10-21)

M0 (QEMU observability bench) is concluded for AIC v1 and v2: AIC trace events,
the `apple-aic-test` and `apple-aic-v2-test` machines, an 8/8 qtest, and
bare-metal payloads that take UART IRQs through AIC and a timer via AIC (v1) or
FIQ (v2). M0b (M1 Pro hardware execution) is open: the payload is cross-compiled
but has not been booted. M1's model portion is complete: multi-CPU, targeting,
AICv1 MMIO IPIs, mask/SW/hardware-state/info registers, 64-bit MMIO, and reset;
qtest 15/15; the v1 SMP/IPI payload passes. The Linux AIC driver probe moves to
M2. See [`m0/README.md`](../m0/README.md),
[`m1/README.md`](../m1/README.md), and
[`notes/2026-10-21-m1-pro-live-hardware.md`](../notes/2026-10-21-m1-pro-live-hardware.md).

## Goal and definition of done

**Goal:** boot Windows 11 ARM64 (pinned build) to a functional desktop on one
pinned Apple Silicon configuration, starting from a prepared image, with no
persistent EL2 hypervisor after handoff.

**Functional desktop:** all CPU cores online, timer and IPIs working, storage,
display, and keyboard/mouse usable.

**Targets:**

- Primary hardware: `MacBookPro18,1` (Apple M1 Pro, t600x, AIC v2).
- Secondary hardware: M1 Mac mini (`j274`, T8103, AIC v1) and M1 MacBook Air
  (`j313`, AIC v1).
- Windows baseline: Windows 11 ARM64 24H2 (build 26100); 25H2 secondary.
- Model: `apple_aic` from the pinned `jprx/qemu-sptm` checkout, with v1 and v2
  test machines.

**Not success:** reaching a setup screen, a kernel handoff, or a boot under an
EL2 shim. The threshold is a desktop with SMP, timer, storage, display, and
input, plus the no-EL2 verification protocol.

## M0 — QEMU observability bench (concluded)

**Goal:** make the AIC model measurable on QEMU and build the bare-metal oracle
payload.

Tasks:

- Replace the `aic_log` no-op with QEMU trace events for read, write, raise, and
  EOI.
- Add a qtest for the AIC register map.
- Write a bare-metal AIC payload: enable a UART IRQ, perform EOI, and exercise
  the timer via AIC (v1) or FIQ (v2).
- Run the payload on the QEMU v1 and v2 machines.
- Capture v1 and v2 register traces.

Exit criteria:

- AIC v1 and v2 register traces captured.
- qtest passes for v1 and v2 (8/8).
- Payloads pass on the QEMU v1 and v2 machines.
- The no-EL2 verification harness has a defined output format.

Dependencies: none.  
Main risk: treating model behavior as hardware truth.

**Status (2026-10-21):** concluded on the QEMU bench. The Darwin-machine trace
was substituted with the AIC test-machine traces; the Darwin machine requires an
IPSW firmware bundle. Hardware execution is M0b; IPI is M1.

## M0b — M1 Pro hardware execution (open)

**Goal:** run the AIC v2 payload on the M1 Pro under `m1n1` and validate the
hardware assumptions.

Tasks:

- Settle `m1n1` payload packaging and the load address (`payload-m1pro.elf`
  currently links at `0x80000000`).
- Tethered `m1n1` boot with a second host (preferred), or a persistent boot
  object if no second host is available.
- Validate Apple UART/PMGR initialization; adjust clock, pinmux, or the
  dockchannel UART if needed.
- Validate AIC v2 UART IRQ `1097` and the FIQ timer on hardware.
- Capture EL2 state before and after handoff and record the no-EL2 evidence.

Exit criteria:

- Payload boots at EL1 under `m1n1` and prints over the Apple UART.
- UART IRQ `1097` and the FIQ timer are observed, or the blockers are
  documented.
- EL2 state is recorded and the no-EL2 protocol result is recorded.
- `m1n1` load address and packaging are documented.

Dependencies: M0; second host; sacrificial M1 Pro; explicit approval to reboot.

Main risk: UART0 is disabled in the device tree and PMGR/clock/pinmux are
unverified; a boot-chain change can lose data if something goes wrong.

## M1 — Extend the AIC model

**Goal:** a multi-CPU AIC model that a general OS can drive.

Tasks:

- Add `num-cpus`; per-CPU IRQ and FIQ outputs; per-IRQ target CPU.
- Add IPI set/clear handling (deferred from M0).
- Add mask set/clear, hardware-state, and info registers.
- Derive the real interrupt count from the ADT/property instead of hard-coding
  `0x1000`.
- Add 64-bit MMIO where the controller allows it.
- Add reset behavior.

Exit criteria:

- Bare-metal SMP and IPI tests pass in QEMU.
- Linux AIC driver probe (deferred to M2; requires the general-purpose machine
  and device tree).

Dependencies: M0 (M0b can run in parallel).

Main risk: AIC semantics that Linux uses but the model omits.

**Status (2026-10-21):** model portion complete. Multi-CPU, target registers,
AICv1 MMIO IPIs, mask/SW/hw-state/info registers, 64-bit MMIO, and reset are
implemented; qtest is 15/15; the v1 SMP/IPI payload passes (3/3). AICv2 fast
IPIs use system registers and are not modeled. The Linux probe is M2.

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

**Goal:** verify the model against a real Apple Silicon Mac (M1 Pro primary).

Tasks:

- Use a sacrificial Apple Silicon Mac (M1 Pro primary) with `m1n1` serial and a
  second host.
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

M0 is concluded. The next useful change set is the first half of M1:

1. `num-cpus` and per-CPU IRQ lines.
2. IPI set/clear and target registers.
3. A qtest and a bare-metal SMP/IPI payload.
4. A minimal `apple-aic-virt` machine that boots a Linux device tree.

That is enough to produce a credible AIC model before investing in UEFI, ACPI,
and the Windows intervention.

## Decision points

- **After M0b:** did the payload boot on hardware and produce UART/FIQ evidence;
  if not, what is blocked?
- **After M3:** is the model faithful enough to justify Windows work?
- **After M4:** is there a viable seam — adapter or patch — or does the
  intervention require broad changes?
- **After M5:** is the result maintainable, or is a minimal EL2 layer the
  permanent answer without vendor support?
- **After M6:** does the no-EL2 protocol pass on hardware; if not, what exactly
  remains at EL2?
