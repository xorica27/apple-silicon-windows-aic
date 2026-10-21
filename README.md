# Hypervisor-Free Windows on Apple Silicon

**Research concept — not a product, not an installer.**

Can Windows 11 ARM64 boot on an Apple Silicon Mac with no EL2 hypervisor shim?
This repository investigates the narrow technical gate that stands in the way:
Windows' HAL assumes an ARM GIC/GICD, while Apple Silicon exposes a proprietary
AIC. Everything else — storage, display, GPU, power — is a driver problem. This
is the HAL problem.

## Status

Concept and upstream survey (October 2026). There is no Windows patch here.
Current community projects (NTASP, NWOAS, `paulsmir/windows-on-m1`,
WatariBoot) boot Windows under an EL2 vGIC shim. This project asks whether that
layer can be removed, and what it would take.

## Key finding

The blocker is architectural, not "just a driver." Microsoft's ARM64 HAL
initializes the root interrupt controller before any normal driver can load, and
expects a GIC Distributor in the MADT. The historical HAL-extension route for
interrupt controllers was removed before Windows 8 RTM. Without Microsoft
support or a patched HAL, the only way to give Windows a GIC is to emulate one —
which is the EL2 shim.

## Documents

- [Research concept](docs/research-concept.md)
- [Prior art](docs/prior-art.md)
- [Upstream survey](docs/upstream-survey.md)
- [Roadmap](docs/roadmap.md)
- [Upstream source inspection, 2026-10-21](notes/2026-10-21-upstream-inspection.md)

## Repository layout

```text
docs/       research concept, prior art, upstream survey, roadmap
notes/      dated lab and inspection notes
scripts/    fetch-upstream.sh (slim checkout of the QEMU AIC model)
upstream/   cloned upstream sources (gitignored)
```

## Quick start

```bash
./scripts/fetch-upstream.sh
```

This shallow-clones `jprx/darwin-vm` and its `qemu-sptm` submodule, pinned in the
[inspection note](notes/2026-10-21-upstream-inspection.md). The upstream QEMU
fork is GPL-2.0 and is deliberately not vendored here.

## Warnings

- Research only. Booting experiments can destroy disks and macOS installs.
- No Microsoft or Apple binaries are included or distributed.
- Nothing here is supported, authorized, or endorsed by Apple or Microsoft.
- Upstream QEMU and Linux AIC code is GPL-2.0. Keep it separate from any
  proprietary Windows patch.

## License

Original content is MIT-licensed. Upstream components retain their own licenses.
See [LICENSE](LICENSE).
