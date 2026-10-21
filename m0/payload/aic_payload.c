#include <stdint.h>
#include <stddef.h>

#define UART_BASE       0x20010000UL
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

#define AIC_BASE        0x20000000UL
#define AIC_NUM_INTS    0x0004
#define AIC_IACK        0x2004
#define AIC_ACK_BASE    0x4080
#define AIC_DISABLE_BASE 0x4100
#define AIC_ENABLE_BASE 0x4180

#define UART_IRQ        100
#define TIMER_IRQ       27

static volatile int uart_irq_seen;
static volatile int timer_irq_seen;

static unsigned current_el(void)
{
    uint64_t value;

    __asm__ volatile("mrs %0, CurrentEL" : "=r"(value));
    return (unsigned)((value >> 2) & 3);
}

static inline void mmio_write32(uintptr_t addr, uint32_t value)
{
    *(volatile uint32_t *)addr = value;
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    return *(volatile uint32_t *)addr;
}

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

static void aic_ack(uint32_t irq)
{
    mmio_write32(AIC_BASE + AIC_ACK_BASE + (irq / 32) * 4,
                 1u << (irq % 32));
}

static uint32_t aic_iack(void)
{
    return mmio_read32(AIC_BASE + AIC_IACK);
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
    __asm__ volatile("msr daifclr, #2" ::: "memory");
}

static void disable_irqs(void)
{
    __asm__ volatile("msr daifset, #2" ::: "memory");
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
    } else if (irq == TIMER_IRQ) {
        timer_stop();
        timer_irq_seen = 1;
    }

    uart_puts("IRQ ");
    uart_puthex(irq);
    uart_puts("\n");

    aic_ack(irq);
    uart_puts("EOI\n");
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
    uart_puts("M0 AIC payload\n");
    uart_puts("CurrentEL=");
    uart_puthex(current_el());
    uart_puts("\n");

    num_ints = mmio_read32(AIC_BASE + AIC_NUM_INTS);
    uart_puts("AIC num_ints=");
    uart_puthex(num_ints);
    uart_puts("\n");

    uart_puts("UART: enable irq ");
    uart_puthex(UART_IRQ);
    uart_puts("\n");
    aic_enable(UART_IRQ);
    uart_enable_tx_irq();
    uart_putc('.');
    enable_irqs();

    for (volatile int i = 0; i < 10000000 && !uart_irq_seen; i++) {
    }
    if (!uart_irq_seen) {
        uart_puts("FAIL uart irq\n");
        for (;;) {
        }
    }
    uart_puts("UART irq ok\n");
    aic_disable(UART_IRQ);
    uart_disable_tx_irq();

    uart_puts("TIMER: enable irq ");
    uart_puthex(TIMER_IRQ);
    uart_puts("\n");
    aic_enable(TIMER_IRQ);
    timer_start();

    for (volatile int i = 0; i < 10000000 && !timer_irq_seen; i++) {
    }
    if (!timer_irq_seen) {
        uart_puts("FAIL timer irq\n");
        for (;;) {
        }
    }
    uart_puts("TIMER irq ok\n");
    aic_disable(TIMER_IRQ);

    uart_puts("IPI: deferred to M1 (single-CPU model)\n");
    uart_puts("PASS\n");

    disable_irqs();
    for (;;) {
        __asm__ volatile("wfi");
    }
}
