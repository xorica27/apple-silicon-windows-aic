# M0 trace notes

Full QEMU trace logs are generated locally and ignored by git (`*.log`). The
filtered sample in `sample-trace.txt` is tracked.

## Capture

```bash
python3 m0/run_payload.py --trace-file m0/trace/aic-trace.log
```

The trace backend writes to the given file, so the serial output stays clean.

## Filter

The PL011 model updates its IRQ line on many register accesses, so the raw trace
is dominated by `apple_aic_irq_request irq=100 level=0`. Filter to the
meaningful events:

```bash
grep -E "apple_aic_(read|write|raise|eoi|enable|disable|ack)" \
  m0/trace/aic-trace.log
```

Repeated `apple_aic_raise` lines while the handler prints before EOI are
expected: the AIC output remains asserted until the ack write clears the pending
bit.

## What the sample shows

- `num_ints` read: `0x100` (256).
- UART IRQ 100: enable, raise, IACK `0x10064` (IRQ 100 plus the valid bit),
  ack, and a second IACK returning 0.
- Timer IRQ 27: enable, raise, IACK `0x1001b`, ack, and a second IACK returning 0.
- Disable writes for both IRQs.
