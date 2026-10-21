# M0 trace notes

Full QEMU trace logs are generated locally and ignored by git (`*.log`). The
filtered samples `sample-trace.txt` (v1) and `sample-trace-v2.txt` (v2) are
tracked.

## Capture

```bash
python3 m0/run_payload.py --board v1 --trace-file m0/trace/aic-trace.log
python3 m0/run_payload.py --board v2 --trace-file m0/trace/aic-v2-trace.log
```

The trace backend writes to the given file, so the serial output stays clean.

## Filter

The PL011 model updates its IRQ line on many register accesses, so the raw trace
is dominated by `apple_aic_irq_request irq=... level=0`. Filter to the
meaningful events:

```bash
grep -E "apple_aic_(read|write|raise|eoi|enable|disable|ack)" \
  m0/trace/aic-v2-trace.log
```

Repeated `apple_aic_raise` lines while the handler prints before EOI are
expected: the AIC output remains asserted until the pending bit is cleared.

## What the v1 sample shows

- `num_ints` read: `0x100` (256).
- UART IRQ 100: enable, raise, IACK `0x10064` (IRQ 100 plus the valid bit),
  ack, and a second IACK returning 0.
- Timer IRQ 27: enable, raise, IACK `0x1001b`, ack, and a second IACK returning 0.
- Disable writes for both IRQs.

## What the v2 sample shows

- `num_ints` read at `0x0C`: `0x1000` (4096).
- UART IRQ 1097: enable at `0x6688` value `0x200`, raise, IACK at `0xC000`
  returning `0x10449`, and a second IACK returning 0. On AIC v2 the IACK read
  clears the pending bit; there is no separate ack-region write.
- Disable at `0x6488`.
- The timer is delivered as **FIQ**, so it does not appear in the AIC trace.
