# Patches

`0001-m0-observability.patch` is **cumulative**: it contains the M0 QEMU
observability bench, the M1 SMP/IPI model changes, and the M2 `apple-aic-virt`
machine. It applies cleanly to `jprx/qemu-sptm` at
`bad6336fb24211f8c4e8e946c259bd5b12417340`.

Apply it with `scripts/apply-m0-patches.sh` after fetching upstream.

The patch includes:

- AIC trace events.
- `apple_aic_create_simple()`.
- Multi-CPU AIC model: per-CPU IRQ outputs, target masks, AICv1 MMIO IPIs,
  mask/SW/hardware-state/info registers, 64-bit MMIO, reset.
- `apple-aic-test` (v1) and `apple-aic-v2-test` (M1 Pro layout) machines.
- The qtest, parameterized for v1 and v2.
- `apple-aic-virt`: RAM, up to 4 CPUs, AICv1 with 256 IRQs, PL011, spin-table
  secondary boot, and per-CPU IRQ/FIQ wiring.

The M2 device tree, initramfs `init`, and boot driver are in
[`m2/`](../m2/); they are not part of the QEMU patch.
