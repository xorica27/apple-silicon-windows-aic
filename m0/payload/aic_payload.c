#include <stdint.h>
#include <stddef.h>

#include "board.h"

static volatile int uart_irq_seen;
static volatile int timer_irq_seen;

static void uart_putc(char c);

static inline void mmio_write32(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

static unsigned current_el(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, CurrentEL" : "=r"(value));
    return (unsigned)((value >> 2) & 3);
}

static void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

static void uart_puthex(uint32_t value)
{
    static const char hex[] = "0123456789abcdef";

    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4) {
        uart_putc(hex[(value >> shift) & 0xf]);
    }
}

#if UART_KIND_PL011
#define UART_DR         0x00
#define UART_FR         0x18
#define UART_IBRD       0x24
#define UART_FBRD       0x28
#define UART_LCR_H      0x2C
#define UART_CR         0x30
#define UART_IMSC       0x38
#define UART_ICR        0x44

#define UART_FR_TXFF    (1u << 5)
#define UART_IMSC_TXIM  (1u << 5)

static void uart_init(void)
{
    mmio_write32(UART_BASE + UART_CR, 0);
    mmio_write32(UART_BASE + UART_ICR, 0x7ff);
    mmio_write32(UART_BASE + UART_IBRD, 13);
    mmio_write32(UART_BASE + UART_FBRD, 0);
    mmio_write32(UART_BASE + UART_LCR_H, 0x70);
    mmio_write32(UART_BASE + UART_CR, 0x301);
}

static void uart_putc(char c)
{
    while (mmio_read32(UART_BASE + UART_FR) & UART_FR_TXFF) {
    }
    mmio_write32(UART_BASE + UART_DR, (uint32_t)(unsigned char)c);
}

static void uart_enable_tx_irq(void)
{
    mmio_write32(UART_BASE + UART_IMSC,
                 mmio_read32(UART_BASE + UART_IMSC) | UART_IMSC_TXIM);
}

static void uart_disable_tx_irq(void)
{
    mmio_write32(UART_BASE + UART_IMSC,
                 mmio_read32(UART_BASE + UART_IMSC) & ~UART_IMSC_TXIM);
}

static void uart_clear_irq(void)
{
    mmio_write32(UART_BASE + UART_ICR, 0x7ff);
}

#elif UART_KIND_APPLE
/* Apple S5L UART, registers from m1n1 src/uart_regs.h */
#define APPLE_ULCON     0x000
#define APPLE_UCON      0x004
#define APPLE_UFCON     0x008
#define APPLE_UTRSTAT   0x010
#define APPLE_UFSTAT    0x018
#define APPLE_UTXH      0x020
#define APPLE_URXH      0x024
#define APPLE_UBRDIV    0x028
#define APPLE_UFRACVAL  0x02c

#define APPLE_UCON_TXTHRESH_ENA (1u << 13)
#define APPLE_UCON_RXTHRESH_ENA (1u << 12)
#define APPLE_UCON_RXTO_ENA     (1u << 9)
#define APPLE_UCON_TXMODE_IRQ   (1u << 2)
#define APPLE_UCON_RXMODE_IRQ   (1u << 0)

#define APPLE_UTRSTAT_RXTO      (1u << 9)
#define APPLE_UTRSTAT_TXTHRESH  (1u << 5)
#define APPLE_UTRSTAT_RXTHRESH  (1u << 4)
#define APPLE_UTRSTAT_TXE       (1u << 2)
#define APPLE_UTRSTAT_TXBE      (1u << 1)
#define APPLE_UTRSTAT_RXD       (1u << 0)

#define PMGR_PS_ACTIVE          0xf
#define PMGR_PS_ACTUAL_MASK     0xf0
#define PMGR_PS_TARGET_MASK     0x0f
#define PMGR_AUTO_ENABLE        (1u << 28)
#define PMGR_WAS_CLKGATED       (1u << 9)
#define PMGR_WAS_PWRGATED       (1u << 8)

static void pmgr_set_mode(uintptr_t addr)
{
    uint32_t value = mmio_read32(addr);

    value &= ~(PMGR_AUTO_ENABLE | PMGR_WAS_CLKGATED | PMGR_WAS_PWRGATED |
               PMGR_PS_TARGET_MASK);
    value |= PMGR_PS_ACTIVE;
    mmio_write32(addr, value);

    for (int i = 0; i < 100000; i++) {
        if ((mmio_read32(addr) & PMGR_PS_ACTUAL_MASK) ==
            (PMGR_PS_ACTIVE << 4)) {
            return;
        }
    }
}

static void apple_uart_power_on(void)
{
    /* t600x pmgr_east: ps_sio 0x180 -> ps_uart_p 0x230 -> ps_uart0 0x240 */
    pmgr_set_mode(PMGR_EAST_BASE + 0x180);
    pmgr_set_mode(PMGR_EAST_BASE + 0x230);
    pmgr_set_mode(PMGR_EAST_BASE + 0x240);
}

static void uart_init(void)
{
    apple_uart_power_on();

    mmio_write32(UART_BASE + APPLE_UCON, 0);
    mmio_write32(UART_BASE + APPLE_ULCON, 0x3);   /* 8N1 */
    mmio_write32(UART_BASE + APPLE_UFCON, 0x1);   /* FIFO enable */
    mmio_write32(UART_BASE + APPLE_UBRDIV, 12);   /* 115200 @ 24 MHz */
    mmio_write32(UART_BASE + APPLE_UFRACVAL, 0);
    mmio_write32(UART_BASE + APPLE_UCON,
                 APPLE_UCON_TXMODE_IRQ | APPLE_UCON_RXMODE_IRQ);
    mmio_write32(UART_BASE + APPLE_UTRSTAT,
                 APPLE_UTRSTAT_TXTHRESH | APPLE_UTRSTAT_RXTHRESH |
                 APPLE_UTRSTAT_RXTO);
}

static void uart_putc(char c)
{
    while (!(mmio_read32(UART_BASE + APPLE_UTRSTAT) & APPLE_UTRSTAT_TXBE)) {
    }
    mmio_write32(UART_BASE + APPLE_UTXH, (uint32_t)(unsigned char)c);
}

static void uart_enable_tx_irq(void)
{
    uint32_t ucon = mmio_read32(UART_BASE + APPLE_UCON);

    ucon |= APPLE_UCON_TXTHRESH_ENA;
    mmio_write32(UART_BASE + APPLE_UCON, ucon);
}

static void uart_disable_tx_irq(void)
{
    uint32_t ucon = mmio_read32(UART_BASE + APPLE_UCON);

    ucon &= ~APPLE_UCON_TXTHRESH_ENA;
    mmio_write32(UART_BASE + APPLE_UCON, ucon);
}

static void uart_clear_irq(void)
{
    mmio_write32(UART_BASE + APPLE_UTRSTAT,
                 APPLE_UTRSTAT_TXTHRESH | APPLE_UTRSTAT_RXTHRESH |
                 APPLE_UTRSTAT_RXTO);
}
#endif

static void aic_enable(uint32_t irq)
{
    mmio_write32(AIC_BASE + AIC_ENABLE_BASE + (irq / 32) * 4,
                 1u << (irq % 32));
}

static void aic_disable(uint32_t irq)
{
    mmio_write32(AIC_BASE + AIC_DISABLE_BASE + (irq / 32) * 4,
                 1u << (irq % 32));
}

static uint32_t aic_iack(void)
{
    return mmio_read32(AIC_BASE + AIC_IACK);
}

static void aic_ack(uint32_t irq)
{
#if AIC_HAS_ACK_REGION
    mmio_write32(AIC_BASE + AIC_ACK_BASE + (irq / 32) * 4,
                 1u << (irq % 32));
#else
    (void)irq; /* IACK read clears the pending bit on AIC v2 */
#endif
}

static inline uint64_t read_cntfrq(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}

static inline void write_cntp_tval(uint64_t value)
{
    __asm__ volatile("msr cntp_tval_el0, %0" :: "r"(value));
}

static inline void write_cntp_ctl(uint64_t value)
{
    __asm__ volatile("msr cntp_ctl_el0, %0" :: "r"(value));
}

static void timer_start(void)
{
    uint64_t freq = read_cntfrq();

    if (!freq) {
        freq = 24000000;
    }
    write_cntp_tval(freq / 100);
    write_cntp_ctl(1);
}

static void timer_stop(void)
{
    write_cntp_ctl(0);
}

static void enable_irqs(void)
{
#if TIMER_FIQ
    __asm__ volatile("msr daifclr, #3" ::: "memory");
#else
    __asm__ volatile("msr daifclr, #2" ::: "memory");
#endif
}

static void disable_irqs(void)
{
    __asm__ volatile("msr daifset, #3" ::: "memory");
}

void c_irq_handler(void)
{
    uint32_t iack = aic_iack();

    if (!iack) {
        return;
    }

    uint32_t irq = iack & 0xffff;

    if (irq == UART_IRQ) {
        uart_disable_tx_irq();
        uart_clear_irq();
        uart_irq_seen = 1;
    }
#if !TIMER_FIQ
    else if (irq == AIC_TIMER_IRQ) {
        timer_stop();
        timer_irq_seen = 1;
    }
#endif

    uart_puts("IRQ ");
    uart_puthex(irq);
    uart_puts("\n");

    aic_ack(irq);
    uart_puts("EOI\n");
}

void c_fiq_handler(void)
{
#if TIMER_FIQ
    timer_stop();
    timer_irq_seen = 1;
    uart_puts("FIQ timer\n");
#else
    uart_puts("unexpected FIQ\n");
#endif
}

void c_default_handler(void)
{
    uart_puts("EXCEPTION\n");
    for (;;) {
        __asm__ volatile("wfi");
    }
}

int main(void)
{
    uint32_t num_ints;

    uart_init();
    uart_puts("M0 AIC payload board=" BOARD_NAME "\n");
    uart_puts("CurrentEL=");
    uart_puthex(current_el());
    uart_puts("\n");

    num_ints = mmio_read32(AIC_BASE + AIC_NUM_INTS);
    uart_puts("AIC version=");
    uart_puthex(AIC_VERSION);
    uart_puts(" num_ints=");
    uart_puthex(num_ints);
    uart_puts("\n");

    uart_puts("UART: enable irq ");
    uart_puthex(UART_IRQ);
    uart_puts("\n");
    aic_enable(UART_IRQ);
    uart_enable_tx_irq();
    uart_putc('.');
    enable_irqs();

    while (!uart_irq_seen) {
        __asm__ volatile("wfi");
    }
    uart_puts("UART irq ok\n");
    aic_disable(UART_IRQ);
    uart_disable_tx_irq();

#if TIMER_FIQ
    uart_puts("TIMER: FIQ\n");
#else
    uart_puts("TIMER: enable irq ");
    uart_puthex(AIC_TIMER_IRQ);
    uart_puts("\n");
    aic_enable(AIC_TIMER_IRQ);
#endif

    timer_start();
    while (!timer_irq_seen) {
        __asm__ volatile("wfi");
    }
    uart_puts("TIMER irq ok\n");

#if !TIMER_FIQ
    aic_disable(AIC_TIMER_IRQ);
#endif

    uart_puts("IPI: deferred to M1 (single-CPU model)\n");
    uart_puts("PASS\n");

    disable_irqs();
    for (;;) {
        __asm__ volatile("wfi");
    }
}
