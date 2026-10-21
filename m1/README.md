# Milestone 1 — AIC model extensions (SMP, IPIs, targeting)

Status: **Model portion complete on QEMU; the Linux AIC driver probe is deferred
to M2** (it requires the general-purpose machine and device tree).

M1 turns the single-CPU, IRQ-only AIC model into a multi-CPU controller that
supports targeting, IPIs, masking, software-set interrupts, hardware-state
reads, 64-bit MMIO, and reset.

## Results

| Check | Result |
|---|---|
| qtest (`apple-aic-test`) | **15/15 PASS** (v1 8, v2 7) |
| v1 SMP/IPI payload | **PASS** (3/3 runs) |
| v2 regression payload | **PASS** (3/3 runs) |
| M1 Pro hardware payload | builds; not executed |
| Linux AIC driver probe | deferred to M2 |

## What M1 added

- **Multi-CPU AIC:** per-CPU IRQ outputs, `num_cpus`, per-IRQ target masks.
- **Targeting:**
  - AIC v1 target registers at `0x3000 + 4*irq` (value is a CPU bitmask).
  - AIC v2 `IRQ_CFG` at `0x2000 + 4*irq` (target field is a CPU index).
- **IPIs (AIC v1 MMIO):** `AIC_IPI_SEND`, `AIC_IPI_ACK`,
  `AIC_IPI_MASK_SET/CLR`, and the per-CPU `AIC_CPU_IPI_*` aliases.
  AIC v2 uses fast-IPI system registers and is not modeled here.
- **Masking and software interrupts:** `SW_SET`, `SW_CLR`, `MASK_SET`,
  `MASK_CLR`, and `HW_STATE` reads.
- **Info registers:** AIC v1 `INFO`, AIC v2 `VERSION`, `INFO1`, `INFO3`,
  `CONFIG`.
- **64-bit MMIO** reads/writes (split into two 32-bit accesses).
- **Reset:** clears enable/pending/target/IPI state and lowers all CPU lines.
- **Test machine:** multiple CPUs, per-CPU IRQ/FIQ wiring, secondary CPU entry.
- **Payload:** CPU1 secondary entry, per-CPU stacks, CPU0→CPU1 IPI test.

The QEMU changes are in the cumulative
[`0001-m0-observability.patch`](../patches/0001-m0-observability.patch) (M0 plus
M1). Apply it with `scripts/apply-m0-patches.sh`.

## Build and run

```bash
./scripts/fetch-upstream.sh
./scripts/apply-m0-patches.sh
./m0/build_qemu.sh

cd m0/payload
make
make payload-m1pro.elf
```

Run the qtest (15 tests, two CPUs):

```bash
cd /tmp/qemu-m0-build
QTEST_QEMU_BINARY=./qemu-system-aarch64 \
  ./tests/qtest/apple-aic-test --tap -k
```

Run the SMP/IPI payload:

```bash
cd m0
python3 run_payload.py --board v1
```

Expected output:

```text
AIC payload board=qemu-v1
CurrentEL=0x00000001
AIC version=0x00000001 num_ints=0x00000100
UART: enable irq 0x00000064
.IRQ 0x00000064
EOI
UART irq ok
TIMER: enable irq 0x0000001b
IRQ 0x0000001b
EOI
TIMER irq ok
SMP: wait for CPU1
SMP: send IPI
CPU IPI
SMP/IPI ok
PASS
```

Capture the v1 trace:

```bash
python3 run_payload.py --board v1 --trace-file trace/aic-v1-smp-trace.log
```

The filtered IPI sequence is in
[`m1/trace/sample-trace-smp.txt`](trace/sample-trace-smp.txt).

## Notes

- **Linux AIC driver probe is M2.** The model side is ready; booting Linux needs
  the general-purpose `apple-aic-virt` machine and a device tree (M2).
- **AICv2 fast IPIs are not modeled.** AICv1 uses MMIO IPI registers; AICv2
  uses `SYS_IMP_APL_IPI_RR_*` system registers. The v2 payload prints a
  deferral line and skips IPI; the v1 payload exercises the MMIO path.
- **qtest uses `-smp 2`** so target tests can address CPU1. The v1 SMP payload
  uses `-smp 2`; the v2 payload uses one CPU.
- **The payload banner is now `AIC payload board=...`**, since the same payload
  covers M0 and M1.

## Files

```text
m1/
  README.md
  trace/sample-trace-smp.txt
```
