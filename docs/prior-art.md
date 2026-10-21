# Prior art

A map of public work relevant to the AIC gate. Status is as of October 2026 and
is based on public repositories and documentation. Self-reported claims are
marked as such.

None of these projects is a complete hypervisor-free desktop boot. They are
relevant because they establish the baseline, the failure modes, and the
components this project can reuse.

## Windows-on-Apple-Silicon projects

| Project | Approach | Hardware | Status | Relevance |
|---|---|---|---|---|
| [NTASP / NT-for-ASi](https://github.com/NT-for-ASi) | Project Mu UEFI port plus a Windows-oriented `m1n1` fork with a GIC emulator | M1/M2 families | Oldest public effort. UEFI boots from USB-C; Windows Boot Manager creates a RAMDisk, then hangs on kernel handoff. AIC and installer listed as not working. | Closest to the native path; provides UEFI/ACPI scaffolding and the `m1n1` fork |
| [NWOAS](https://github.com/cornch-k/NWOAS) | `m1n1` EL2 mediation, guest `m1n1`, Project Mu, Windows on internal SSD | M1 Mac mini | Reached the Windows desktop (Sep 2026): 8 cores, 15.08 GB RAM, Cinebench 1888. Emulated NVMe, second Mac for boot orchestration, GPU/network/power incomplete. Explicitly not hypervisor-free. | Proves Windows can run at speed under a shim; documents the remaining device gaps |
| [`paulsmir/windows-on-m1`](https://github.com/paulsmir/windows-on-m1) | Project Mu fork plus `m1n1` EL2 fork | M1 MacBook Air (j313) | Self-reported desktop, 8 cores, synthetic NVMe, USB xHCI, keyboard/trackpad, framebuffer, D3D11/OpenGL, partial D3D12. | Shows how far a derivative EL2 stack can get; claims are not independently verified |
| [WatariBoot](https://github.com/ZonatedCord/WatariBoot) | Native-first research; now a minimal EL2 layer with virtual GICv3/PSCI | M1 target | Windows 11 ARM64 reaches the lock screen in QEMU TCG (Oct 2026); not yet on real hardware. Documents why a fully native path failed (F-05, F-14). | The clearest public analysis of the GIC/HAL blocker; its failure log is essential reading |
| [SiliconWin](https://github.com/devdasx/SiliconWin) | VM app using Apple Hypervisor.framework and QEMU | M1–M5 | Virtualization, not bare metal. | Useful baseline for "near-native speed in a VM," not for this problem |

## Platform and interrupt-controller work

| Project | What it provides | License | Relevance |
|---|---|---|---|
| [Asahi Linux](https://asahilinux.org/) | Reverse-engineered Apple Silicon platform support, including the AIC driver | GPL-2.0 | The semantic oracle for AIC; `m1n1` is the bootloader used by every project |
| Linux `irq-apple-aic.c` | Register-level AIC driver: masking, targeting, IPIs, FIQ, EOI | GPL-2.0 | Primary reference for the model and the HAL patch |
| [`m1n1`](https://github.com/AsahiLinux/m1n1) | Early hardware init, AIC setup, IPIs, serial proxy | GPL-2.0 | Boot chain and AIC behavior on real hardware |
| Apple DT bindings (`apple,aic`, `apple,aic2`) | Register layout and affinity properties | Kernel documentation | Spec for the model and ACPI/DT tables |
| [`jprx/darwin-vm`](https://github.com/jprx/darwin-vm) | QEMU harness that boots iOS/macOS on emulated M1–M5 | MIT wrapper; QEMU submodule GPL-2.0 | Working Apple Silicon machine plumbing and a reusable AIC device model |
| [`jprx/qemu-sptm`](https://github.com/jprx/qemu-sptm) | QEMU fork with `apple_aic.c`, Apple device-tree parsing, XNU boot | GPL-2.0 | The existing AIC model this project extends |
| QEMU `vmapple` machine | Models the device model Apple's Virtualization.framework exposes to macOS guests | GPL-2.0 | Useful for Apple virtual-platform conventions, but it is not an AIC platform |
| [ChefKiss/Inferno](https://github.com/ChefKissInc/Inferno) and forks | QEMU Apple device emulation, currently iPhone 11 | GPL-2.0 | Additional Apple device-model work; AIC coverage unclear |

## Microsoft documentation

| Document | Relevance |
|---|---|
| [ACPI system description tables](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/acpi-system-description-tables) | Windows inbox GIC/GICD support and MADT expectations |
| [UEFI requirements for SoC platforms](https://learn.microsoft.com/en-us/windows-hardware/drivers/bringup/uefi-requirements-that-apply-to-all-windows-platforms) | Multiprocessor Parking Protocol as the non-PSCI CPU bring-up path |
| [Windows ARM64 interrupt internals](https://connormcgarr.github.io/windows-arm64-interrupts/) | Public analysis of the Windows ARM64 GIC model |

## What the prior art proves

- Windows 11 ARM64 can run on Apple Silicon at full CPU speed under an EL2
  vGIC/PSCI shim.
- The device gap is large but tractable in stages: storage, input, display, and
  partial GPU acceleration have all been demonstrated in derivative projects.
- The GIC/GICD assumption is the boot gate. A MADT without a GICD freezes the
  kernel, and the old HAL-extension route for interrupt controllers is gone.

## What no public project proves

- That Windows can boot with **no persistent EL2 layer** and reach a functional
  desktop.
- That the Windows HAL can be patched, or a GIC-to-AIC adapter built, in a
  maintainable way.
- That a QEMU AIC model is faithful enough for Windows without hardware
  validation.
- That ACPI parking protocol plus AIC is sufficient for SMP, timer, and power
  management together.
- That "no persistent EL2" can be verified rather than merely asserted.
