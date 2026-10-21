# Milestone 0 — Observability and oracle

Status: **QEMU bench complete for AIC v1 and v2; M1 Pro hardware payload
cross-compiled; hardware execution pending.**

M0 makes the AIC model measurable and gives the project a bare-metal oracle. On
the QEMU side, trace events, two test machines (v1 and v2), an 8-test qtest, and
bare-metal payloads all pass. The M1 Pro hardware payload is built and ready for
an `m1n1` run, but has not been executed on hardware.

## Results

| Check | Result |
|---|---|
| QEMU fork builds (`aarch64-softmmu`) | PASS |
| `apple-aic-test` machine (AIC v1) | PASS |
| `apple-aic-v2-test` machine (M1 Pro layout) | PASS |
| qtest (`apple-aic-test`) | 8/8 PASS (4 v1 + 4 v2) |
| Bare-metal payload, QEMU v1 | PASS |
| Bare-metal payload, QEMU v2 | PASS |
| M1 Pro (`t600x`) payload | builds; **not executed** |
| AIC trace captured (v1 and v2) | PASS |
| No-EL2 report format and validator | DEFINED |

## What was added

- AIC trace events: read, write, enable, disable, ack, raise, EOI, and IRQ
  request.
- `apple_aic_create_simple()` for test machines without an Apple device tree.
- Two machines:
  - `apple-aic-test`: AIC v1, base `0x20000000`, IACK `0x2004`, UART IRQ 100,
    timer through AIC IRQ 27.
  - `apple-aic-v2-test`: M1 Pro layout — AIC v2, base `0x28E100000`, size
    `0x50000`, IACK `0xC000`, enable `0x6600`, disable `0x6400`, 4096 IRQs,
    UART at `0x39B200000` with IRQ `1097`, timer via **FIQ**.
- qtest for the AIC register map, parameterized for v1 and v2.
- Payload board configurations in `payload/board.h`:
  - `BOARD_QEMU_V1` — PL011, AIC v1, timer via AIC.
  - `BOARD_QEMU_V2` — PL011, AIC v2, UART IRQ 1097, timer via FIQ.
  - `BOARD_M1PRO` — Apple S5L UART, PMGR power-on, AIC v2, UART IRQ 1097,
    timer via FIQ.
- FIQ vector and handler in `payload/start.S`.

The QEMU changes are exported as
[`patches/0001-m0-observability.patch`](../patches/0001-m0-observability.patch)
and can be applied with `scripts/apply-m0-patches.sh` after fetching upstream.

## Build and run

Fetch upstream and apply the patch:

```bash
./scripts/fetch-upstream.sh
./scripts/apply-m0-patches.sh
./m0/build_qemu.sh
```

Build the payloads:

```bash
cd m0/payload
make                    # payload-v1.elf, payload-v2.elf
make payload-m1pro.elf  # hardware variant (not executed)
```

Run the qtest (8 tests):

```bash
cd /tmp/qemu-m0-build
QTEST_QEMU_BINARY=./qemu-system-aarch64 \
  ./tests/qtest/apple-aic-test --tap -k
```

Run the QEMU payloads:

```bash
cd m0
python3 run_payload.py --board v1
python3 run_payload.py --board v2
```

Expected v2 serial output:

```text
M0 AIC payload board=qemu-v2
CurrentEL=0x00000001
AIC version=0x00000002 num_ints=0x00001000
UART: enable irq 0x00000449
.IRQ 0x00000449
EOI
UART irq ok
TIMER: FIQ
FIQ timer
TIMER irq ok
IPI: deferred to M1 (single-CPU model)
PASS
```

Capture the v2 trace:

```bash
python3 run_payload.py --board v2 --trace-file trace/aic-v2-trace.log
```

## M1 Pro hardware payload

The available test machine is `MacBookPro18,1` (Apple M1 Pro, `t600x`, AIC v2).
Live inspection is recorded in
[`notes/2026-10-21-m1-pro-live-hardware.md`](../notes/2026-10-21-m1-pro-live-hardware.md).
Key values used by `BOARD_M1PRO`:

| Item | Value |
|---|---|
| AIC | v2, base `0x28E100000`, IACK `0xC000` |
| UART0 | `0x39B200000`, AIC IRQ `1097` |
| Timer | `arm,armv8-timer` via **FIQ** |
| PMGR | `pmgr_east` `0x28E580000`; `ps_sio` `0x180`, `ps_uart_p` `0x230`, `ps_uart0` `0x240` |

Important caveats:

- macOS/XNU owns AIC v2 at EL1. The payload **cannot** run while macOS is
  booted; it must run instead of macOS under `m1n1` (tethered boot preferred).
- UART0 is `status = "disabled"` in the Linux device tree. The Apple UART and
  PMGR initialization is best-effort and unverified; output may require clock,
  pinmux, or dockchannel-UART adjustments.
- `payload-m1pro.elf` links at `0x80000000` (see `linker-hw.ld`). The `m1n1`
  payload packaging/load address is still to be settled.
- No hardware execution has been attempted.

## Notes and deviations

- **IPI is deferred to M1.** Both QEMU machines are single-CPU and the AIC model
  has no IPI registers. The payload prints a deferral line.
- **The Darwin machine trace was not captured.** The `darwin` machine needs an
  IPSW firmware bundle; M0 captures traces from the AIC test machines instead.
- **Hardware run is pending.** The M1 Pro payload builds but has not been booted.

## Files

```text
m0/
  README.md
  build_qemu.sh
  run_payload.py
  payload/            board.h, start.S, aic_payload.c, linker scripts
  trace/              trace notes and filtered v1/v2 samples
  no_el2/             no-persistent-EL2 report schema, validator, sample
```
