# Milestone 2 — `apple-aic-virt` boots Linux

Status: **complete.** Linux 6.12 boots to userspace on the AIC virtual machine
with all four CPUs online, a 24 MHz arch timer delivered through the CPU FIQ
path, and AICv1 MMIO IPIs working across CPUs.

This is the first milestone where a general-purpose OS, rather than a bare-metal
payload, drives the AIC model.

## Results

| Check | Result |
|---|---|
| Linux reaches userspace | **PASS** (`M2: USERSPACE REACHED`) |
| `console=ttyAMA0` | **PASS** (PL011, Linux IRQ 35) |
| SMP | **4/4 CPUs online** (`M2: CPUS online=0-3`) |
| Timer | **PASS** (24 MHz virtual timer, Linux IRQ 34, `AIC-FIQ`) |
| AICv1 MMIO IPIs | **PASS** (nonzero reschedule/function-call IPIs on all CPUs) |
| Reliability | **3/3 runs** (two 4-CPU, one 2-CPU) |
| AICv2 boot | not yet tested |

Boot excerpt:

```text
[    0.086691] smp: Bringing up secondary CPUs ...
[    0.110923] CPU1: Booted secondary processor 0x0000000001 [0x000f0510]
[    0.126634] CPU2: Booted secondary processor 0x0000000002 [0x000f0510]
[    0.132061] CPU3: Booted secondary processor 0x0000000003 [0x000f0510]
[    0.132858] smp: Brought up 1 node, 4 CPUs
...
M2: USERSPACE REACHED
M2: CPUS online=0-3 count=4 expected=4
...
 34:        370        291        302        391   AIC-FIQ   1 Level     arch_timer
 35:          0          0          0          0       AIC 65636 Level     uart-pl011
IPI0:        25         10         28         13       Rescheduling interrupts
IPI1:       290        257         70        189       Function call interrupts
...
M2: PASS
```

The full boot log is in [`linux-boot.txt`](linux-boot.txt).

An AIC trace from a 4-CPU boot records the MMIO IPI path end to end:
**987** `apple_aic_ipi_send`, **987** `apple_aic_ipi_event`, **987**
`apple_aic_ipi_raise`, and **991** `apple_aic_ipi_ack` events. Ordinary AIC IRQ
raises are zero because the only device IRQ (PL011) is idle and the timer uses
FIQ. A filtered sequence is in
[`trace/sample-trace-ipi.txt`](trace/sample-trace-ipi.txt).

## Machine

`apple-aic-virt` is a small ARM64 machine built around the AIC model, not a
clone of QEMU `virt`:

| Region | Address | Notes |
|---|---|---|
| RAM | `0x40000000` | 256 MiB default |
| AIC | `0x20000000` | 64 KiB, AICv1, 256 IRQs |
| PL011 | `0x20010000` | IRQ 100 → Linux IRQ 35 |
| Spin mailbox | `0x45000000` | 64 KiB reserved window |
| Spin stub | `0x45001000` | four-instruction spin-table stub |
| Timer | CPU `GTIMER_VIRT` | wired to CPU FIQ; Linux IRQ 34 |

The DTS describes AIC, PL011, the four CPUs, the timer, and a fixed 24 MHz
clock. The machine runs Linux at EL1: EL2 and EL3 are disabled on the CPUs, so
there is no hypervisor or secure monitor in the boot path.

The AIC is instantiated with `apple_aic_create_simple(AIC_V1, 256, num_cpus,
0x8000, 0x2004)` and the standard v1 layout: `INFO` at `0x4`, `WHOAMI` at
`0x2000`, `EVENT`/`IACK` at `0x2004`, target registers at `0x3000`, and the
IPI/mask/SW registers after them.

## Two bugs found and fixed

### 1. Linux uses the virtual timer PPI, not the physical one

The first boot stalled immediately after:

```text
arch_timer: cp15 timer(s) running at 24.00MHz (phys).
Spurious percpu IRQ33 on CPU0
```

The machine originally wired `GTIMER_PHYS` to the CPU FIQ. But Linux at EL1
cannot use EL2's physical timer registers, so `arch_timer_select_ppi()` chooses
the virtual PPI. The DT's interrupt order matters: the driver reads index 1 as
the non-secure physical timer and index 2 as the virtual timer, and the AIC
driver remaps `AIC_TMR_GUEST_VIRT` to `AIC_TMR_EL0_VIRT` at EL1. The physical
timer FIQ therefore had no registered action and stormed as a spurious per-CPU
IRQ.

Fix:

- Reorder the timer node to the binding's `secure phys, non-secure phys, virt,
  hyp phys` order.
- Wire `GTIMER_VIRT` to the CPU FIQ, not `GTIMER_PHYS`.

The timer is now reported as `24.00MHz (virt)` and handled as Linux IRQ 34.

This is a deliberate bench shortcut: the AIC model does not model FIQ routing or
FIQ status. Linux's FIQ handler identifies the timer source by reading
`CNTV_CTL_EL0`, so a direct timer-to-FIQ wire is behaviorally equivalent for
this test. It is not a claim that AIC FIQ routing has been validated.

### 2. The spin-table mailbox was inside the kernel image

The first SMP attempt left CPUs 1–3 offline, each after a 6.6 s timeout. The
spin mailbox was at `0x40000000` and the stub at `0x4007F000`. A 6.12 arm64
`Image` loads at `0x40000000` and ends at `0x443D0000`, so both were inside the
kernel image and were overwritten during boot.

Fix:

- Move the mailbox to `0x45000000` and the stub to `0x45001000`.
- Reserve `0x45000000`–`0x4500FFFF` with `/memreserve/` so Linux never allocates
  it.

The kernel's spin-table code writes the secondary entry point to the mailbox and
the stub branches to it. All four CPUs now come online in about 40 ms.

## Reproduce

### 1. Build QEMU

```bash
./scripts/fetch-upstream.sh
./scripts/apply-m0-patches.sh
./m0/build_qemu.sh
```

The cumulative patch now includes `hw/arm/apple_aic_virt.c` and its
`Kconfig`/`meson.build` entries.

### 2. Build Linux

```bash
git clone --depth 1 -b v6.12 \
  https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git \
  /tmp/linux-m2/linux-6.12
cd /tmp/linux-m2/linux-6.12
make ARCH=arm64 CROSS_COMPILE=aarch64-elf- defconfig
scripts/config \
  --enable APPLE_AIC --enable SERIAL_AMBA_PL011 \
  --enable DEVTMPFS --enable BLK_DEV_INITRD \
  --disable MODULES
make ARCH=arm64 CROSS_COMPILE=aarch64-elf- olddefconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-elf- -j"$(nproc)" Image
```

Confirm `CONFIG_APPLE_AIC=y` in `.config`. On a Linux host this is sufficient.
The reference build was cross-compiled on macOS; that host needed extra
host-tool shims (`elf.h`, `byteswap.h`, and a `uuid_t` collision in
`scripts/mod/file2alias.c`). Those are host workarounds, not kernel changes for
the AIC.

### 3. Build the initramfs

```bash
curl -LO https://dl-cdn.alpinelinux.org/alpine/v3.20/releases/aarch64/alpine-minirootfs-3.20.3-aarch64.tar.gz
mkdir -p /tmp/m2-initramfs
tar -xzf alpine-minirootfs-3.20.3-aarch64.tar.gz -C /tmp/m2-initramfs
cp m2/init /tmp/m2-initramfs/init
chmod 755 /tmp/m2-initramfs/init
(cd /tmp/m2-initramfs && find . -print | cpio -o -H newc | gzip -9) \
  > /tmp/m2-initramfs.cpio.gz
```

`m2/init` requires the number of CPUs passed as `m2.expected_cpus=<n>` and
fails if not all of them are online. `run_linux.py` adds that automatically.

### 4. Compile the DTB

```bash
clang -E -nostdinc -I /tmp/linux-m2/linux-6.12/include \
  -undef -D__DTS__ -x assembler-with-cpp -P \
  m2/apple-aic-virt.dts -o /tmp/apple-aic-virt.dts.pp
dtc -I dts -O dtb -o /tmp/apple-aic-virt.dtb /tmp/apple-aic-virt.dts.pp
```

### 5. Boot

```bash
python3 m2/run_linux.py --smp 4
```

Useful options: `--smp`, `--memory`, `--timeout`, `--log`, and
`--trace-file`. The default kernel, initramfs, and DTB paths are the `/tmp`
locations above.

Expected tail:

```text
M2: USERSPACE REACHED
M2: CPUS online=0-3 count=4 expected=4
...
M2: PASS
```

## Caveats

- **AICv1 only.** This machine uses `compatible = "apple,aic"` and 256 IRQs.
  Booting Linux on the AICv2 layout is a separate step; the M1 Pro target is
  AICv2. The model and DTS support the v2 register layout, but no `apple-aic2`
  Linux boot has been run yet.
- **The timer FIQ bypasses the AIC.** FIQ routing/status is not modeled; the
  timer output is wired directly to the CPU FIQ. The AIC event path is used for
  ordinary IRQs, but the only ordinary device IRQ in this machine is the PL011,
  and it never fires because no RX input is injected. AICv1 IPIs are real and
  exercised by Linux.
- **No RTC, `fw_cfg`, or virtio.** Userspace is an initramfs. These devices are
  listed in the M2 task list but are not required by the exit criterion; they
  move to the storage/validation work (M3/M4).
- **No EL2/EL3.** Linux runs at EL1 by design. The no-EL2 verification protocol
  is still a hardware milestone (M0b/M6).
- **No binaries are committed.** The kernel `Image`, initramfs, and DTB are
  build artifacts under `/tmp`; only sources and scripts are in this repository.

## Files

```text
m2/
  README.md
  apple-aic-virt.dts     machine device tree
  init                   initramfs init: checks CPUs, prints AIC/timer/UART state
  run_linux.py           boot driver with PASS/FAIL detection
  linux-boot.txt         successful 4-CPU boot log
  trace/sample-trace-ipi.txt  filtered AICv1 MMIO IPI sequence
```
