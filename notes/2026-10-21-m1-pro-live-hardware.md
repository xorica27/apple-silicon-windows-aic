# M1 Pro live hardware inspection — 2026-10-21

Read-only inspection of the available test machine while macOS was running.
No boot-chain changes, no AIC register writes, no reboots.

## Host

- Model: `MacBookPro18,1`
- Chip: Apple M1 Pro, 10 cores (8 performance, 2 efficiency)
- Memory: 16 GB
- macOS: 15.7.5
- Loaded interrupt driver: `com.apple.driver.AppleInterruptControllerV2`

## AIC

Live IORegistry (`ioreg -p IODeviceTree -r -n aic -l -w 0`):

```text
+-o aic@8E100000  <class AppleARMIODevice>
     "compatible" = <"aic,2">
     "aic-iack-offset" = <00c0000000000000>
     "IODeviceMemory" = (({"address"=10973347840,"length"=327680}))
     "device_type" = <"interrupt-controller">
```

Decoded:

| Field | Value |
|---|---|
| AIC version | v2 (`aic,2`, `AppleInterruptControllerV2`) |
| Base | `0x28E100000` (`10973347840`) |
| Mapped size | `0x50000` (`327680`) |
| IACK / event | `0x28E10C000` (offset `0xC000`) |

Linux `t600x-die0.dtsi` describes the same controller as:

```text
aic: interrupt-controller@28e100000 {
    compatible = "apple,t6000-aic", "apple,aic2";
    reg = <0x2 0x8e100000 0x0 0xc000>,
          <0x2 0x8e10c000 0x0 0x4>;
    reg-names = "core", "event";
    #interrupt-cells = <4>;
    interrupt-controller;
};
```

This matches the live IACK offset: the event register is at `+0xC000`.

## UART

Linux `t600x-die0.dtsi`:

```text
serial0: serial@39b200000 {
    compatible = "apple,s5l-uart";
    reg = <0x3 0x9b200000 0x0 0x1000>;
    reg-io-width = <4>;
    interrupts = <AIC_IRQ 0 1097 IRQ_TYPE_LEVEL_HIGH>;
    power-domains = <&ps_uart0>;
    clocks = <&clkref>, <&clkref>;
    clock-names = "uart", "clk_uart_baud0";
    status = "disabled";
};
```

- Base: `0x39B200000`
- AIC IRQ: `1097`
- The node is disabled by default. The live macOS device tree only exposed a
  different `dockchannel-uart@92528000`; UART0 may require PMGR/clock/pin setup
  before it can be used by a bare-metal payload.

## UART power/clock

Linux `t600x-pmgr.dtsi` and `t600x-dieX.dtsi`:

- `pmgr_east` base: `0x28E580000`
- Power domains: `ps_sio` at `0x180`, `ps_uart_p` at `0x230`,
  `ps_uart0` at `0x240`
- PMGR bits (from `m1n1/src/pmgr.c`): target `GENMASK(3,0)`, actual
  `GENMASK(7,4)`, active `0xf`, `AUTO_ENABLE BIT(28)`, `WAS_CLKGATED BIT(9)`,
  `WAS_PWRGATED BIT(8)`.

The hardware payload enables `ps_sio` → `ps_uart_p` → `ps_uart0` in that order
and polls the actual power state.

## Timer

`t600x-common.dtsi` uses `arm,armv8-timer` with `AIC_FIQ` interrupt lines
(`AIC_TMR_GUEST_PHYS`, `AIC_TMR_GUEST_VIRT`, `AIC_TMR_HV_PHYS`,
`AIC_TMR_HV_VIRT`). On Apple Silicon the timer is delivered as **FIQ**, not as an
AIC IRQ. The hardware payload therefore has a FIQ vector and does not ack the
timer through AIC.

## Apple UART register notes

From `m1n1/src/uart_regs.h`:

```text
ULCON 0x000, UCON 0x004, UFCON 0x008, UTRSTAT 0x010,
UFSTAT 0x018, UTXH 0x020, URXH 0x024, UBRDIV 0x028, UFRACVAL 0x02c

UCON_TXTHRESH_ENA BIT(13), UCON_RXTHRESH_ENA BIT(12), UCON_RXTO_ENA BIT(9)
UCON_TXMODE GENMASK(3,2), UCON_RXMODE GENMASK(1,0), mode 1 = IRQ
UTRSTAT_TXTHRESH BIT(5), UTRSTAT_RXTHRESH BIT(4), UTRSTAT_RXE BIT(2),
UTRSTAT_TXBE BIT(1), UTRSTAT_RXD BIT(0)
```

## Implications

1. The earlier QEMU bench was AIC v1. It is not representative of this host.
   An AIC v2 machine and payload were added and pass in QEMU.
2. The v1 payload cannot run on this Mac. The `payload-m1pro.elf` target uses
   AIC v2, UART IRQ 1097, Apple UART registers, PMGR power-on, and FIQ timer.
3. macOS owns AIC v2 at EL1; the payload cannot run while macOS is booted.
   A real hardware run requires booting `m1n1` instead of macOS (tethered boot
   preferred; boot-object install is persistent and riskier).
4. UART0 is `status = "disabled"` in Linux. The Apple UART/PMGR initialization in
   the hardware payload is best-effort and unverified; output on hardware may
   need adjustment (clock, pinmux, or the dockchannel UART).
5. The hardware payload is cross-compiled but **not executed**. m1n1 payload
   packaging/load address still needs to be settled. This work is tracked as
   **M0b**, separate from the concluded M0 QEMU bench.

## Commands used

```bash
ioreg -p IODeviceTree -r -n aic -l -w 0
kmutil showloaded | grep -i AppleInterruptController
curl -fsSL https://raw.githubusercontent.com/torvalds/linux/master/arch/arm64/boot/dts/apple/t600x-die0.dtsi
curl -fsSL https://raw.githubusercontent.com/torvalds/linux/master/arch/arm64/boot/dts/apple/t600x-dieX.dtsi
curl -fsSL https://raw.githubusercontent.com/torvalds/linux/master/arch/arm64/boot/dts/apple/t600x-pmgr.dtsi
```
