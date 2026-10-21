# Bare-Metal Windows 11 on Apple Silicon: The AIC Gate

**Status:** Research concept, pre-implementation  
**Date:** 2026-10-21  
**Author:** xorica27  
**License:** MIT for original content; upstream components retain their own licenses.

## Abstract

The goal is to boot Windows 11 ARM64 to a functional desktop on Apple Silicon
with **no persistent EL2 hypervisor** — bare metal, not a VM, and not the current
EL2 shim. The decisive boot gate is architectural: Windows' ARM64 HAL initializes
the root interrupt controller before any normal driver can load and expects an
ARM GIC/GICD, while Apple Silicon exposes the proprietary AIC. There is no
supported third-party interrupt-controller extension point, and public
experiments show that a MADT without a GICD freezes the kernel.

This project therefore studies the AIC gate as the critical path to a broader
goal. It does not claim that solving AIC is sufficient: storage, display, input,
power, and GPU drivers are the larger work behind the gate. It does claim that
without a solution to the AIC gate, a hypervisor-free boot is impossible, and
that the size and maintainability of any solution can be measured rather than
assumed.

## 1. Goal, scope, and definition of done

### 1.1 Goal

> Boot Windows 11 ARM64 (pinned build) to a **functional desktop** on one pinned
> Apple Silicon configuration, starting from a prepared image, with **no
> persistent EL2 hypervisor after handoff**.

"Functional desktop" means:

- all CPU cores online;
- timer and IPIs working;
- storage, display, and keyboard/mouse usable;
- Windows reaches the desktop without an EL2 shim.

Installer/OOBE, GPU acceleration, and production support are out of scope. The
target is "boot a prepared image to a usable desktop," not "ship a dual-boot
product."

### 1.2 What "native" means

"Native" is overloaded. This project uses a three-level distinction:

| Mode | What runs beneath Windows | In scope? |
|---|---|---|
| **VM** (Parallels, UTM, VMware) | macOS + a hypervisor | No |
| **Bare metal with EL2 shim** | `m1n1` at EL2 presenting a virtual GIC/PSCI | Baseline, not the goal |
| **Hypervisor-free bare metal** | Firmware/bootloader only; hands off at EL1 | **Goal** |

A bootloader and firmware may still run before Windows. What must be absent is a
**persistent hypervisor at EL2** after handoff:

- no EL2 code running after `ExitBootServices`;
- no stage-2 translation;
- no virtual GIC or PSCI emulation;
- AIC programmed directly by the OS.

"Native" here does **not** mean "supported, licensed, or endorsed by Apple or
Microsoft."

### 1.3 Success threshold

The project has two success thresholds:

- **Model success (M5):** patched Windows reaches a functional desktop on the
  QEMU AIC machine.
- **Hardware success (M6):** the same image reaches a functional desktop on a
  real M1, and the absence of a persistent EL2 layer is verified.

A boot to a setup screen or a kernel handoff is not success. The threshold is a
desktop with SMP, timer, storage, display, and input.

### 1.4 Targets

- **Primary hardware:** M1 Mac mini (`j274`, T8103, AIC v1). It is the cheapest
  sacrificial target and the configuration used by NWOAS.
- **Secondary hardware:** M1 MacBook Air (`j313`, T8103), for built-in display,
  keyboard, and trackpad experiments.
- **Windows baseline:** Windows 11 ARM64 24H2 (build 26100). 25H2 is a secondary
  target. The patch is build-specific; every experiment records the build number.
- **Model:** the `apple_aic` device model in the pinned `jprx/qemu-sptm`
  checkout.

M2+ uses AIC v2/v3 and is a separate support matrix; it is not part of the
initial success threshold.

### 1.5 Non-goals

- A production installer or supported dual-boot product.
- Distributing patched Windows images or binaries.
- GPU acceleration in early phases.
- Supporting every Apple SoC or every Windows build.
- Replacing macOS as the primary OS.
- A boot-to-setup-screen demo as the end state.

## 2. The gate: why AIC, not "just drivers"

A normal Windows driver cannot solve the interrupt-controller problem because of
where the requirement sits in the boot sequence:

```text
firmware -> Windows Boot Manager -> ntoskrnl + HAL
         -> HAL initializes the root interrupt controller
         -> PnP loads drivers (.sys)
         -> user mode
```

The root interrupt controller is initialized by the HAL before any normal
driver can load. A boot-start driver is still too late. A UEFI driver is gone
after `ExitBootServices`. The practical options are:

1. **Native AIC support in the Windows ARM64 HAL** (or a supported
   HAL-extension point for interrupt controllers).
2. **A GIC-shaped adapter** that redirects the HAL's GIC operations to AIC.
3. **A patched HAL/`ntoskrnl`** that drives AIC directly.
4. **A Microsoft-supported vendor path** for non-GIC ARM64 platforms.
5. **Emulating a GIC** for Windows — the EL2 shim.

WatariBoot's research reports that the historical HAL-extension interface for
interrupt controllers was removed before Windows 8 RTM, leaving "no official
route" ([F-05](https://github.com/ZonatedCord/WatariBoot/blob/main/docs/FAILURES.md)).
Its experiment with a MADT that has no GICD loaded `ntoskrnl` and then froze
indefinitely ([F-14](https://github.com/ZonatedCord/WatariBoot/blob/main/docs/FAILURES.md)).
That is the gate this project studies.

The AIC gate is necessary but not sufficient. Even with AIC solved, a functional
desktop needs storage, display, input, and power work. Those are ordinary — if
very large — driver problems. The gate decides whether a hypervisor-free boot is
possible at all; the drivers decide whether the result is usable.

## 3. Background

### 3.1 Boot chain and exception levels

Apple Silicon boots through Boot ROM and iBoot. The Asahi Linux project's
`m1n1` bootloader is installed as a boot object and is the usual entry point for
non-Apple OS research. On ARM, EL0 is user mode, EL1 is the OS kernel, EL2 is a
hypervisor, and EL3 is secure firmware. In the current Windows projects,
Windows runs at EL1 while `m1n1` remains at EL2 and mediates interrupts and CPU
startup.

"No EL2 shim" in this document means **no persistent hypervisor at EL2**, not
"no bootloader": `m1n1` may still run before Windows and hand off at EL1.

### 3.2 AIC versus GIC

The ARM GIC is a standardized interrupt controller with a distributor (GICD),
CPU interfaces (GICC), redistributors (GICR), and in GICv3 an ITS for MSIs. Its
interrupt model uses SPIs, PPIs, and SGIs, and it is described to the OS through
ACPI MADT structures.

Apple's AIC is a different design. It has its own register layout, event
numbering, masking, targeting, and IPI mechanism. Linux supports it through
`drivers/irqchip/irq-apple-aic.c`; `m1n1` contains the low-level initialization
and IPI code; the Apple device-tree bindings document the register and affinity
properties (`apple,aic`, and newer `apple,aic2` variants).

The two controllers are not register-compatible. A driver written for one does
not drive the other without a translation layer.

### 3.3 What Windows ARM64 expects

Microsoft documents that Windows includes inbox support for the GIC and GIC
Distributor, and that they are described in the ACPI MADT
([ACPI system description tables](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/acpi-system-description-tables)).
PSCI is the normal CPU bring-up mechanism, but it is not strictly mandatory:
Microsoft requires the **ACPI Multiprocessor Parking Protocol** on ARM platforms
that do not support PSCI
([UEFI requirements](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/uefi-requirements-that-apply-to-all-windows-platforms)).
That gives a native path for initial SMP. It does not solve CPU idle, suspend,
or shutdown, which normally use PSCI.

### 3.4 Current community state

As of October 2026:

- **NTASP** (NT for Apple Silicon Project) maintains a Project Mu UEFI port and
  a Windows-oriented `m1n1` fork with a GIC emulator. Its public mainline boots
  the Windows bootloader and creates a RAMDisk, then hangs on handoff to the
  kernel. Its feature table lists the AIC path as partial, the GPU as not
  working, NVMe as in progress, and the installer as not working.
- **NWOAS** boots Windows 11 ARM64 to the desktop on an M1 Mac mini from the
  internal SSD, under `m1n1` EL2 mediation with an emulated NVMe interface and a
  second Mac for boot orchestration. It explicitly says it is not
  hypervisor-free bare metal.
- **`paulsmir/windows-on-m1`** reports an M1 MacBook Air reaching the desktop
  with eight cores, a synthetic NVMe controller, USB xHCI, keyboard/trackpad,
  framebuffer, and partial GPU acceleration, again under an `m1n1` EL2
  hypervisor fork. These claims are self-reported and not independently
  verified.
- **WatariBoot** is a native-first research project that concluded Windows
  "insists on a real GIC" and that the HAL-extension route is gone; it now uses
  a minimal EL2 layer with a virtual GICv3. It reached the Windows lock screen
  in QEMU TCG, not on real hardware.

No public project implements AIC in the Windows HAL, and no public project has
demonstrated a verified hypervisor-free Windows boot with a functional desktop.

## 4. Research questions

- **RQ1.** What is the smallest viable intervention that lets Windows initialize
  interrupts on AIC: a GIC-shaped adapter, a HAL patch, or a vendor-supported
  path?
- **RQ2.** Which AIC semantics (targeting, FIQ, IPIs, masking, EOI, priorities)
  must a model implement to be faithful enough for Windows bring-up?
- **RQ3.** Can the intervention be maintained across Windows builds, or does it
  require Microsoft support?
- **RQ4.** After interrupts work, what remains for a functional desktop
  (parking protocol, timer, storage, display, input, power)?
- **RQ5.** Is a hypervisor-free boot achievable at all without vendor
  cooperation, or is a minimal EL2 layer permanent?
- **RQ6.** How can "no persistent EL2" be verified rather than asserted?

## 5. Hypotheses

- **H1.** A bounded intervention exists — either a GIC-to-AIC adapter or a small
  HAL patch — that lets Windows initialize interrupts on AIC. Which is smaller
  and more maintainable is an empirical question, not an assumption.
- **H2.** The MADT/GICD requirement is the hard gate; a dummy GICD is not
  sufficient, because Windows requires the distributor to be functional, not
  merely present.
- **H3.** A QEMU AIC model can reproduce enough behavior to develop and debug
  the intervention, but hardware validation is mandatory for timing, FIQ, and
  SoC-specific quirks.
- **H4.** The ACPI parking protocol can replace PSCI for initial SMP, but CPU
  idle, suspend, and shutdown remain gaps that need ACPI power-management work.
- **H5.** Linux-on-AIC is a necessary intermediate oracle: if Linux cannot run
  cleanly on the model, Windows results are not credible.
- **H6.** A hypervisor-free boot can be demonstrated by verifying the absence of
  EL2 code, stage-2 translation, and vGIC/PSCI emulation after handoff. If that
  verification fails, the honest conclusion is "an EL2 layer remains required."

## 6. Methodology

### 6.1 Oracle

Use Linux `irq-apple-aic.c`, `m1n1`, and the Apple device-tree bindings as the
ground truth for AIC semantics. Build a bare-metal AIC test payload that runs on
`m1n1` hardware and on the QEMU model. If the model and hardware disagree, the
model is wrong.

### 6.2 Instrumentation

- QEMU tracepoints for every AIC MMIO read/write, raise, and EOI.
- A qtest for the AIC register map.
- On hardware, `m1n1` serial and a second host for early boot.
- For Windows, WinDbg/KDNET where a NIC is available, ETW, and crash dumps.
- Record the exact register sequence Windows performs before and after the
  intervention.

### 6.3 Model

Start from `jprx/qemu-sptm`'s `hw/intc/apple_aic.c`, which implements AIC
v1/v2/v3 but is deliberately minimal: one CPU, one IRQ output, enable/disable,
pending, and EOI; no FIQ, IPIs, affinities, or SMP. Extend it with per-CPU IRQ
and FIQ lines, target/affinity, IPI set/clear, mask set/clear, hardware-state
and info registers, 64-bit MMIO where appropriate, and reset behavior.

Build a general-purpose `apple-aic-virt` machine around it, modeled on QEMU's
`virt` machine but with AIC instead of GIC. Boot Linux first with a device tree;
add UEFI/ACPI and Windows-capable devices later.

### 6.4 Intervention design space

| Option | What it changes | Who must act | Trade-off |
|---|---|---|---|
| **Native AIC in HAL** | Windows HAL gains an AIC backend | Microsoft | Cleanest; no vendor incentive |
| **GIC-shaped adapter** | GIC read/write operations are redirected to AIC | Community patch | Likely the smallest seam; semantic mismatches |
| **Microsoft HAL extension** | A signed extension supplies the interrupt controller | Microsoft + vendor | Clean and maintainable; interface removed |
| **Patched HAL/`ntoskrnl`** | Binary patch of the HAL | Community | Fast to prototype; fragile and build-specific |
| **EL2 shim** | `m1n1` presents a virtual GIC/PSCI | Community | Works today; not hypervisor-free |

The research compares the **adapter** and the **HAL patch** first, because they
require no vendor cooperation. Native HAL support and a signed extension are the
preferred long-term outcomes but are outside the project's control.

### 6.5 Patch levels

1. **Offline binary patch** of a local `hal.dll`/`ntoskrnl` image. Best for
   research: easy rollback, no distribution, breaks on updates.
2. **Boot-time in-memory patch** by a loader. More flexible, but resembles a
   rootkit, conflicts with integrity features, and is legally awkward.
3. **HAL extension or vendor support.** The only maintainable path, but no
   supported interface exists today.

Start at level 1 on a lab machine. Treat levels 2 and 3 as separate product and
legal decisions, not research steps.

### 6.6 Validation ladder

```text
bare-metal AIC payload
  -> Linux uniprocessor
  -> Linux SMP (timer, IPIs, affinity)
  -> Linux interrupt/storage stress
  -> stock Windows (reproduce the GICD freeze)
  -> patched Windows (timer, one device IRQ, IPIs, storage, desktop)
  -> real M1 hardware + no-EL2 verification
```

Each rung isolates one class of failure. Do not debug SMP before the timer
works, and do not touch Windows before Linux runs on the model.

### 6.7 Verifying "no persistent EL2"

The claim must be measured, not asserted. The verification protocol is:

1. Before handoff, the bootloader records the EL2 state and confirms it will
   drop to EL1.
2. After `ExitBootServices`, confirm no EL2 vector table is active and no
   stage-2 translation is enabled.
3. Confirm no vGIC or PSCI emulation is running; the OS programs AIC directly.
4. A Windows test driver reads `CurrentEL` and reports EL1. This alone does not
   prove the absence of a hypervisor, so it is combined with an external check
   from firmware/`m1n1` and the absence of EL2 code.
5. The evidence — register dumps, trace logs, and the test driver report — is
   recorded in the lab notebook for each hardware run.

If the protocol cannot be satisfied, the result is reported as "EL2 remains
required," not as a bare-metal success.

## 7. Milestones

| ID | Milestone | Exit criterion |
|---|---|---|
| M0 | Observability and oracle | AIC trace captured; bare-metal payload passes on model and hardware |
| M1 | AIC model extensions | SMP/IPI/FIQ/target tests pass in QEMU |
| M2 | General-purpose machine | Linux boots to userspace with AIC and device tree |
| M3 | Linux validation | 4 CPUs online; timer, IPIs, virtio-blk, and stress pass without lost IRQs |
| M4 | Stock Windows | Reproducible `ntoskrnl` freeze; full trace captured |
| M5 | Patched Windows on model | Functional desktop on the AIC model, with no EL2 shim |
| M6 | Hardware validation | Functional desktop on a sacrificial M1; no-EL2 protocol passes |

Detailed dependencies and decision points are in the [roadmap](roadmap.md).

## 8. Evaluation criteria

- **Boot:** Linux reaches userspace; Windows reaches a functional desktop.
- **SMP:** all cores online; IPIs delivered; no lost or stuck interrupts.
- **Timer:** stable tick, bounded drift, no missed ticks under load.
- **Latency:** interrupt latency and EOI latency measured under stress.
- **Intervention surface:** number of patched functions/sites; stability across
  Windows builds.
- **Driver coverage:** storage, USB input, display; GPU explicitly out of scope
  for early phases.
- **Fidelity:** model-versus-hardware comparison for the same tests.
- **No-EL2 evidence:** the verification protocol in §6.7 passes.

## 9. Minimum publishable result versus stretch result

**Minimum publishable result:**

- a documented, faithful AIC model and general-purpose AIC machine;
- a reproducible Linux-on-AIC test bench with SMP/timer/IPI results;
- a characterization of the Windows HAL's GIC assumptions and the minimal seam;
- a positive or negative answer on hypervisor-free feasibility.

**Stretch result:**

- a patched Windows functional desktop on the QEMU AIC model;
- the same desktop on a real M1;
- a no-EL2 verification report and a model/hardware parity report.

This separation keeps the research publishable even if the ultimate boot fails.

## 10. Risks and mitigations

| Risk | Mitigation |
|---|---|
| No supported HAL extension point | Compare adapter and patch; pursue vendor support in parallel; document the limitation honestly |
| Model fidelity | Differential testing against Linux/`m1n1` on real hardware; mark every unverified behavior |
| GPL contamination | Keep the QEMU/Linux bench separate; reimplement semantics clean-room for any proprietary patch |
| Boot-chain/security changes | Use a sacrificial machine, reduced security, backups, and rollback images |
| Legal exposure | No redistribution of Microsoft binaries; interoperability research only; consult counsel before publishing patches |
| Scope creep | Linux-first milestones; GPU and production installer are non-goals |
| Thermal/power behavior | Not modeled; hardware validation only |
| "Native" claim inflation | Use the §6.7 verification protocol; report failures as failures |

## 11. Expected contributions

- A documented, reusable AIC model and machine for general-OS experiments.
- A reproducible Linux-on-AIC test bench.
- A characterization of the Windows HAL's GIC assumptions and the minimal
  intervention surface.
- A public positive or negative result on hypervisor-free feasibility, with a
  verification protocol.
- A prior-art map and a licensing analysis for the community.

## 12. Legal and ethical considerations

This is interoperability research on hardware the author owns, in the spirit of
Asahi Linux. It does not aim to circumvent DRM, defeat licensing, or distribute
proprietary software. Practical boundaries:

- Do not redistribute Microsoft or Apple binaries, firmware, or Windows images.
- Keep patches local; describe them rather than shipping complete images.
- Respect the Apple and Microsoft EULAs; consult counsel before publishing
  anything that modifies Windows.
- Keep GPL-2.0 code (QEMU, Linux, `m1n1`) separate from proprietary patches.
- Boot experiments can destroy data; use a dedicated machine and backups.

## 13. Glossary

- **AIC:** Apple Interrupt Controller; Apple Silicon's proprietary interrupt
  controller.
- **Bare metal:** the OS runs directly on the hardware, with no OS or hypervisor
  beneath it. It does not mean "driverless."
- **EL0/EL1/EL2/EL3:** ARM exception levels: user, kernel, hypervisor, secure
  firmware.
- **GIC/GICD:** ARM Generic Interrupt Controller and its distributor.
- **HAL:** Windows Hardware Abstraction Layer; owns early interrupt-controller
  initialization.
- **Hypervisor-free:** no persistent EL2 code, stage-2 translation, or vGIC/PSCI
  emulation after handoff; a bootloader may still run before handoff.
- **MADT:** ACPI Multiple APIC Description Table; describes interrupt
  controllers to the OS.
- **Native:** used here as shorthand for hypervisor-free bare metal, not
  "supported by the vendor."
- **Parking protocol:** ACPI mechanism for starting secondary CPUs on ARM
  platforms without PSCI.
- **PSCI:** ARM Power State Coordination Interface; standard CPU bring-up and
  power-management interface.
- **Shim:** in this project, the `m1n1` EL2 layer that presents a virtual GIC and
  PSCI to Windows.

## 14. References

- Asahi Linux: <https://asahilinux.org/>
- Linux `irq-apple-aic.c`:
  <https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/drivers/irqchip/irq-apple-aic.c>
- Apple AIC device-tree binding:
  <https://www.kernel.org/doc/Documentation/devicetree/bindings/interrupt-controller/apple%2Caic.yaml>
- `m1n1`: <https://github.com/AsahiLinux/m1n1>
- `jprx/darwin-vm`: <https://github.com/jprx/darwin-vm>
- `jprx/qemu-sptm`: <https://github.com/jprx/qemu-sptm>
- NTASP: <https://github.com/NT-for-ASi>
- NWOAS: <https://github.com/cornch-k/NWOAS>
- `paulsmir/windows-on-m1`: <https://github.com/paulsmir/windows-on-m1>
- WatariBoot: <https://github.com/ZonatedCord/WatariBoot>
- WatariBoot failure log:
  <https://github.com/ZonatedCord/WatariBoot/blob/main/docs/FAILURES.md>
- Microsoft ACPI system description tables:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/acpi-system-description-tables>
- Microsoft UEFI requirements for SoC platforms:
  <https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/uefi-requirements-that-apply-to-all-windows-platforms>
- Windows ARM64 interrupt internals:
  <https://connormcgarr.github.io/windows-arm64-interrupts/>
