# Contributing

This is a research repository, not a product. Contributions that improve
reproducibility, documentation, prior-art accuracy, or the AIC model are welcome.

## Ground rules

- **No proprietary binaries.** Do not commit Microsoft or Apple binaries,
  firmware, Windows images, or patched Windows files.
- **No redistribution of patched Windows.** Local research patches may be
  described, but not distributed as complete images or binaries.
- **Keep GPL code separate.** QEMU, Linux, and `m1n1` are GPL-2.0. Do not copy
  their code into a proprietary patch; document the semantics or reimplement
  clean-room.
- **Cite sources.** Every hardware claim should point to a public source: Linux,
  `m1n1`, Apple device-tree bindings, Microsoft documentation, or a reproducible
  experiment.
- **Lab safety.** Boot experiments can destroy disks. Document backups, rollback
  images, and the exact hardware used.
- **Be explicit about confidence.** Mark claims as observed, derived, or
  inferred. Self-reported upstream claims are not independent verification.

## Useful contributions

- Corrections or additions to [prior art](docs/prior-art.md).
- Register-level AIC documentation with citations.
- QEMU AIC model fixes or tests.
- Linux-on-AIC reproducibility notes.
- Negative results. A documented failure is a contribution.

## Licensing

By contributing, you agree that original documentation and scripts are licensed
under the repository's MIT license. Upstream code retains its own license.
