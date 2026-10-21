# Hypervisor-Free Windows on Apple Silicon: The AIC Problem

**Status:** Research concept, pre-implementation  
**Date:** 2026-10-21  
**Author:** xorica27  
**License:** MIT for original content; upstream components retain their own licenses.

## Abstract

Apple Silicon Macs cannot boot Windows 11 bare-metal. The usual explanation —
"Apple does not support Boot Camp" or "there are no drivers" — is true but
incomplete. Current community projects can boot Windows 11 ARM64 from an M1's
internal SSD, but only under an EL2 hypervisor that presents a virtual GIC and
PSCI. This project investigates whether that layer can be removed: can Windows'
HAL be made to drive Apple's AIC interrupt controller directly?

We argue the decisive blocker is architectural. Windows' ARM64 HAL assumes an
ARM GIC/GICD and has no supported third-party interrupt-controller extension
point; Apple Silicon exposes the proprietary AIC. Drivers for storage, display,
GPU, power, and networking are a separate, larger problem. The research
program therefore targets the boot gate first, then the driver stack.

## 1. Problem statement

A stock Windows 11 ARM64 image expects a platform that looks like a standard
ARM64 server or Qualcomm laptop: UEFI, ACPI, a GIC interrupt controller, PSCI
for CPU bring-up, and a set of devices with inbox or vendor drivers. Apple
Silicon provides almost none of that:

- no Boot Camp or Apple-supplied Windows firmware/driver stack;
- an undocumented Apple Interrupt Controller (AIC) instead of a GIC;
- no PSCI;
- Apple-specific storage, display, GPU, USB, PCIe/DART, power, and input
  hardware.

Community projects work around the interrupt-controller gap by running `m1n1`
at EL2 and presenting Windows with a virtual GICv3 and PSCI. That layer is the
**EL2 shim**. It makes Windows boot, but it is an extra privileged component,
adds trapping overhead, depends on the Apple boot chain, and is not what a
"bare-metal" port would look like.

The research question is therefore not "can Windows boot on a Mac?" — it can,
under the shim — but **"can it boot without the shim, and if not, what exactly
is missing?"**

## 2. Why this is not "just a driver"

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

1. **Microsoft adds AIC support to the Windows ARM64 HAL** (or restores a
   supported HAL-extension point for interrupt controllers).
2. **Patch the HAL/`ntoskrnl`** to drive AIC instead of a GIC.
3. **Emulate a GIC** for Windows — which is the EL2 shim.

WatariBoot's research reports that the historical HAL-extension interface for
interrupt controllers was removed before Windows 8 RTM, leaving "no official
route" ([F-05](https://github.com/ZonatedCord/WatariBoot/blob/main/docs/FAILURES.md)).
Its experiment with a MADT that has no GICD loaded `ntoskrnl` and then froze
indefinitely ([F-14](https://github.com/ZonatedCord/WatariBoot/blob/main/docs/FAILURES.md)).
That is the gate this project studies.

This does not mean the rest is easy. Even with AIC solved, a usable system needs
storage, display, input, PCIe/DART, power/thermal, networking, and eventually a
GPU driver. Those are ordinary — if very large — driver problems. The
interrupt-controller problem is the one that decides whether a hypervisor-free
boot is possible at all.

## 3. Background

### 3.1 Boot chain and exception levels

Apple Silicon boots through Boot ROM and iBoot. The Asahi Linux project's
`m1n1` bootloader is installed as a boot object and is the usual entry point for
non-Apple OS research. On ARM, EL0 is user mode, EL1 is the OS kernel, EL2 is a
hypervisor, and EL3 is secure firmware. In the current Windows projects,
Windows runs at EL1 while `m1n1` remains at EL2 and mediates interrupts and CPU
startup. "No EL2 shim" in this document means **no persistent hypervisor at
EL2**, not "no bootloader": `m1n1` may still run before Windows and hand off at
EL1.

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
achieved a hypervisor-free Windows boot.

## 4. Research questions

- **RQ1.** What is the minimal Windows HAL patch surface that lets Windows drive
  AIC directly?
- **RQ2.** Which AIC semantics (targeting, FIQ, IPIs, masking, EOI, priorities)
  must a model implement to be faithful enough for Windows bring-up?
- **RQ3.** Can such a patch be maintained across Windows builds, or does it
  require Microsoft support?
- **RQ4.** After interrupts work, what remains for a usable system (parking
  protocol, timer, storage, display, input, power)?
- **RQ5.** Is a hypervisor-free boot achievable at all without vendor
  cooperation, or is a minimal EL2 layer permanent?

## 5. Hypotheses

- **H1.** AIC can be mapped onto Windows' interrupt model with a bounded HAL
  patch — a small number of initialization, ack/EOI, mask/unmask, target, and
  IPI functions, not a rewrite of the HAL.
- **H2.** The MADT/GICD requirement is the hard gate; a dummy GICD is not
  sufficient, because Windows requires the distributor to be functional, not
  merely present.
- **H3.** A QEMU AIC model can reproduce enough behavior to develop and debug
  the patch, but hardware validation is mandatory for timing, FIQ, and
  SoC-specific quirks.
- **H4.** The ACPI parking protocol can replace PSCI for initial SMP, but CPU
  idle, suspend, and shutdown remain gaps that need ACPI power-management work.
- **H5.** Linux-on-AIC is a necessary intermediate oracle: if Linux cannot run
  cleanly on the model, Windows results are not credible.

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
  patch.

### 6.3 Model

Start from `jprx/qemu-sptm`'s `hw/intc/apple_aic.c`, which implements AIC
v1/v2/v3 but is deliberately minimal: one CPU, one IRQ output, enable/disable,
pending, and EOI; no FIQ, IPIs, affinities, or SMP. Extend it with per-CPU IRQ
and FIQ lines, target/affinity, IPI set/clear, mask set/clear, hardware-state
and info registers, 64-bit MMIO where appropriate, and reset behavior.

Build a general-purpose `apple-aic-virt` machine around it, modeled on QEMU's
`virt` machine but with AIC instead of GIC. Boot Linux first with a device tree;
add UEFI/ACPI and Windows-capable devices later.

### 6.4 Patch levels

1. **Offline binary patch** of a local `hal.dll`/`ntoskrnl` image. Best for
   research: easy rollback, no distribution, breaks on updates.
2. **Boot-time in-memory patch** by a loader. More flexible, but resembles a
   rootkit, conflicts with integrity features, and is legally awkward.
3. **HAL extension or vendor support.** The only maintainable path, but no
   supported interface exists today.

Start at level 1 on a lab machine. Treat levels 2 and 3 as separate product and
legal decisions, not research steps.

### 6.5 Validation ladder

```text
bare-metal AIC payload
  -> Linux uniprocessor
  -> Linux SMP (timer, IPIs, affinity)
  -> Linux interrupt/storage stress
  -> stock Windows (reproduce the GICD freeze)
  -> patched Windows (timer, one device IRQ, IPIs, storage, desktop)
  -> real M1 hardware
```

Each rung isolates one class of failure. Do not debug SMP before the timer
works, and do not touch Windows before Linux runs on the model.

## 7. Milestones

| ID | Milestone | Exit criterion |
|---|---|---|
| M0 | Observability and oracle | AIC trace captured; bare-metal payload passes on model and hardware |
| M1 | AIC model extensions | SMP/IPI/FIQ/target tests pass in QEMU |
| M2 | General-purpose machine | Linux boots to userspace with AIC and device tree |
| M3 | Linux validation | 4 CPUs online; timer, IPIs, virtio-blk, and stress pass without lost IRQs |
| M4 | Stock Windows | Reproducible `ntoskrnl` freeze; full trace captured |
| M5 | HAL patch | Patched Windows reaches a desktop on the AIC model |
| M6 | Hardware validation | Same image boots on a sacrificial M1; model/hardware parity report |

The detailed breakdown, dependencies, and risks are in the
[roadmap](roadmap.md).

## 8. Evaluation criteria

- **Boot:** Linux reaches userspace; Windows reaches a desktop.
- **SMP:** all cores online; IPIs delivered; no lost or stuck interrupts.
- **Timer:** stable tick, bounded drift, no missed ticks under load.
- **Latency:** interrupt latency and EOI latency measured under stress.
- **Patch surface:** number of patched functions/sites; stability across Windows
  builds.
- **Driver coverage:** storage, USB input, display; GPU explicitly out of scope
  for early phases.
- **Fidelity:** model-versus-hardware comparison for the same tests.

## 9. Risks and mitigations

| Risk | Mitigation |
|---|---|
| No supported HAL extension point | Treat HAL patching as research; pursue vendor support in parallel; document the limitation honestly |
| Model fidelity | Differential testing against Linux/`m1n1` on real hardware; mark every unverified behavior |
| GPL contamination | Keep the QEMU/Linux bench separate; reimplement semantics clean-room for any proprietary patch |
| Boot-chain/security changes | Use a sacrificial machine, reduced security, backups, and rollback images |
| Legal exposure | No redistribution of Microsoft binaries; interoperability research only; consult counsel before publishing patches |
| Scope creep | Linux-first milestones; GPU and production installer are non-goals |
| Thermal/power behavior | Not modeled; hardware validation only |

## 10. Non-goals

- A production Windows installer or a supported dual-boot product.
- Distributing patched Windows images or binaries.
- GPU acceleration in early phases.
- Supporting every Apple SoC or every Windows build.
- Replacing macOS as the primary OS.

## 11. Expected contributions

- A documented, reusable AIC model and machine for general-OS experiments.
- A reproducible Linux-on-AIC test bench.
- A characterization of the Windows HAL's GIC assumptions and the minimal patch
  surface.
- A public positive or negative result on hypervisor-free feasibility.
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
- **MADT:** ACPI Multiple APIC Description Table; describes interrupt
  controllers to the OS.
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
