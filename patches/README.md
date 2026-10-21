# Patches

`0001-m0-observability.patch` is **cumulative**: it contains the M0 QEMU
observability bench plus the M1 SMP/IPI model changes. It applies cleanly to
`jprx/qemu-sptm` at `bad6336fb24211f8c4e8e946c259bd5b12417340`.

Apply it with `scripts/apply-m0-patches.sh` after fetching upstream.

The patch includes:

- AIC trace events.
- `apple_aic_create_simple()`.
- Multi-CPU AIC model: per-CPU IRQ outputs, target masks, AICv1 MMIO IPIs,
  mask/SW/hardware-state/info registers, 64-bit MMIO, reset.
- `apple-aic-test` (v1) and `apple-aic-v2-test` (M1 Pro layout) machines.
- The qtest, parameterized for v1 and v2.
