# Milestone 0 — Observability and oracle

Status: **QEMU bench complete; hardware run pending.**

M0 makes the AIC model measurable and gives the project a bare-metal oracle.
The QEMU side is done: trace events, a test machine, a qtest, and a payload that
exercises UART and timer interrupts through the AIC and performs EOI.

## Results

| Check | Result |
|---|---|
| QEMU fork builds (`aarch64-softmmu`) | PASS |
| `apple-aic-test` machine registered | PASS |
| qtest (`apple-aic-test`) | 4/4 PASS |
| Bare-metal payload on QEMU | PASS |
| AIC register trace captured | PASS |
| Payload on real M1 via `m1n1` | **PENDING** (no lab machine) |
| No-EL2 report format and validator | DEFINED |

## What was added

- AIC trace events in `hw/intc/trace-events` and `hw/intc/apple_aic.c`:
  read, write, enable, disable, ack, raise, EOI, and IRQ request.
- `apple_aic_create_simple()` for test machines that do not have an Apple
  device tree.
- `apple-aic-test` machine: RAM at `0x40000000`, AIC at `0x20000000`, PL011
  UART at `0x20010000`, test control register at `0x20020000`.
- qtest for the AIC register map: interrupt count, enable/raise/ack, mask, and
  unknown-register behavior.
- Bare-metal payload: reads the interrupt count, enables UART IRQ 100, enables
  the timer IRQ 27, handles both through the AIC, performs EOI, and prints
  `PASS`.

The QEMU changes are exported as
[`patches/0001-m0-observability.patch`](../patches/0001-m0-observability.patch)
and can be applied with `scripts/apply-m0-patches.sh` after fetching upstream.

## Build and run

Fetch upstream first:

```bash
./scripts/fetch-upstream.sh
```

Apply the M0 patch, then build QEMU in a space-free path (QEMU configure rejects
spaces in the source path):

```bash
./scripts/apply-m0-patches.sh
./m0/build_qemu.sh
```

Build the payload:

```bash
cd m0/payload
make
```

Run the qtest:

```bash
cd /tmp/qemu-m0-build
QTEST_QEMU_BINARY=./qemu-system-aarch64 \
  ./tests/qtest/apple-aic-test --tap -k
```

Run the payload:

```bash
cd m0
python3 run_payload.py
```

Expected serial output:

```text
M0 AIC payload
CurrentEL=0x00000001
AIC num_ints=0x00000100
UART: enable irq 0x00000064
.IRQ 0x00000064
EOI
UART irq ok
TIMER: enable irq 0x0000001b
IRQ 0x0000001b
EOI
TIMER irq ok
IPI: deferred to M1 (single-CPU model)
PASS
```

Capture a trace without polluting the serial stream:

```bash
python3 run_payload.py --trace-file trace/aic-trace.log
```

## Notes and deviations

- **IPI is deferred to M1.** The current AIC model is single-CPU and has no IPI
  registers. The payload prints a deferral line so the result is explicit.
  M1 adds SMP and IPI.
- **The Darwin machine trace was not captured.** The `darwin` machine needs an
  IPSW firmware bundle (`get_files.sh` downloads it). M0 captures the trace from
  the `apple-aic-test` machine instead. That machine is the direct precursor to
  M2's general-purpose `apple-aic-virt` machine.
- **Hardware run is pending.** No M1 lab machine is available in this
  environment. The payload is written to run at EL1 with a fixed M1-like memory
  map; the hardware run will need the real AIC/UART addresses and an `m1n1`
  payload packaging step.

## Files

```text
m0/
  README.md
  build_qemu.sh
  run_payload.py
  payload/            bare-metal payload (start.S, aic_payload.c, linker.ld)
  trace/              trace notes and a filtered sample
  no_el2/             no-persistent-EL2 report schema, validator, sample
```
