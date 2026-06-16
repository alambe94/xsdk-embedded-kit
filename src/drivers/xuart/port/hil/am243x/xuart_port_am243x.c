#include "xuart.h"

/* AM243x UART register offsets (word-addressed: each 8-bit reg occupies one 32-bit word) */
#define UART_THR    0x00U   /* Transmit Holding Register (DLAB=0, write) */
#define UART_DLL    0x00U   /* Divisor Latch Low         (DLAB=1) */
#define UART_IER    0x04U   /* Interrupt Enable Register (DLAB=0) */
#define UART_DLH    0x04U   /* Divisor Latch High        (DLAB=1) */
#define UART_FCR    0x08U   /* FIFO Control Register     (write) */
#define UART_LCR    0x0CU   /* Line Control Register */
#define UART_LSR    0x14U   /* Line Status Register */
#define UART_MDR1   0x20U   /* TI Mode Definition Register 1 (not in standard 16550) */

#define UART_LCR_DLAB       (1U << 7)
#define UART_LCR_8N1        0x03U
#define UART_FCR_FIFO_EN    0x07U       /* enable + reset TX and RX FIFOs */
#define UART_LSR_THRE       (1U << 5)
#define UART_MDR1_DISABLE   0x07U       /* MODESELECT=111: IP disabled (reset default) */
#define UART_MDR1_UART16X   0x00U       /* MODESELECT=000: 16x UART (normal polling) */

/* AM243x MAIN_PADCFG_CTRL_MMR - pin mux configuration for UART0 TX/RX.
 * PADCFG base: 0x000F0000, PMUX region starts at +0x4000.
 * UART0_RXD/TXD offsets: from TI csl_pinmux am64x_am243x PIN_UART0_RXD/TXD.
 * KICK unlock required before writing pad config registers (reset = locked).
 * Two partitions: LOCK0 KICK0/1 at +0x1008/0x100C, LOCK1 KICK0/1 at +0x5008/0x500C. */
#define PADCFG_BASE             0x000F0000U
#define PADCFG_PMUX_OFF         0x4000U
#define PADCFG_LOCK0_KICK0_OFF  0x1008U
#define PADCFG_LOCK0_KICK1_OFF  0x100CU
#define PADCFG_LOCK1_KICK0_OFF  0x5008U
#define PADCFG_LOCK1_KICK1_OFF  0x500CU
#define PADCFG_KICK0_UNLOCK     0x68EF3490U
#define PADCFG_KICK1_UNLOCK     0xD172BC5AU
#define PADCFG_KICK_LOCK        0x00000000U

#define UART0_RXD_PAD_OFF   0x0230U
#define UART0_TXD_PAD_OFF   0x0234U

/* Pad config value fields (from TI pinmux.h for am64x_am243x):
 *   bit 16: PULLUDEN  - 1 = disable pull resistor
 *   bit 17: PULLTYPESEL - 1 = pull-up (when pull enabled)
 *   bit 18: INPUT_EN  - 1 = enable input receiver */
#define PAD_MODE(m)         ((uint32_t)(m))
#define PAD_PULL_DISABLE    (1U << 16U)
#define PAD_PULL_UP         (1U << 17U)
#define PAD_INPUT_EN        (1U << 18U)

/* TX: mode 0, pull disabled, output */
#define UART0_TXD_PAD_VAL   (PAD_MODE(0U) | PAD_PULL_DISABLE)
/* RX: mode 0, pull-up enabled, input receiver on */
#define UART0_RXD_PAD_VAL   (PAD_MODE(0U) | PAD_PULL_UP | PAD_INPUT_EN)

#define REG32(base, off) (*(volatile uint32_t *)((base) + (off)))

/* Configure baud rate, 8N1, FIFO, and enable the TI UART IP core.
 * The AM243x UART MDR1 reset default is 0x07 (disabled), so this must be
 * called before any xuart_putc/xuart_puts call.
 * Pass baud_rate=0 or module_clk_hz=0 to skip reconfiguration and only
 * re-enable MDR1 (used by SBL when ROM has already set the baud rate).
 * module_clk_hz = 48 000 000 when SBL/TIFS configured MAIN_PLL0;
 * module_clk_hz = 25 000 000 (HFOSC0) in a bare JTAG session without SBL. */
void xuart_init(uint32_t base_addr, uint32_t baud_rate, uint32_t module_clk_hz)
{
    if (baud_rate == 0U || module_clk_hz == 0U)
    {
        /* Caller guarantees ROM already set up baud rate; just enable the IP. */
        REG32(base_addr, UART_MDR1) = UART_MDR1_UART16X;
        return;
    }

    /* Pinmux: route UART0 TX/RX pads to UART0 function (mode 0).
     * Required in bare JTAG sessions where ROM did not configure the pads.
     * PADCFG is write-protected at reset; unlock both KICK partitions first. */
    REG32(PADCFG_BASE, PADCFG_LOCK0_KICK0_OFF) = PADCFG_KICK0_UNLOCK;
    REG32(PADCFG_BASE, PADCFG_LOCK0_KICK1_OFF) = PADCFG_KICK1_UNLOCK;
    REG32(PADCFG_BASE, PADCFG_LOCK1_KICK0_OFF) = PADCFG_KICK0_UNLOCK;
    REG32(PADCFG_BASE, PADCFG_LOCK1_KICK1_OFF) = PADCFG_KICK1_UNLOCK;

    uint32_t padbase = PADCFG_BASE + PADCFG_PMUX_OFF;
    REG32(padbase, UART0_TXD_PAD_OFF) = UART0_TXD_PAD_VAL;
    REG32(padbase, UART0_RXD_PAD_OFF) = UART0_RXD_PAD_VAL;

    REG32(PADCFG_BASE, PADCFG_LOCK0_KICK0_OFF) = PADCFG_KICK_LOCK;
    REG32(PADCFG_BASE, PADCFG_LOCK1_KICK0_OFF) = PADCFG_KICK_LOCK;

    /* Round to nearest divisor: (clk + 8*baud) / (16*baud).
     * At 25 MHz / 115200 this gives 14 (3.1% error) vs truncated 13 (4.3% error). */
    uint32_t divisor = (module_clk_hz + (8U * baud_rate)) / (16U * baud_rate);
    if (divisor == 0U) divisor = 1U;

    REG32(base_addr, UART_MDR1) = UART_MDR1_DISABLE; /* hold in reset while reconfiguring */

    REG32(base_addr, UART_LCR) = UART_LCR_DLAB;      /* unlock divisor latches */
    REG32(base_addr, UART_DLL) = divisor & 0xFFU;
    REG32(base_addr, UART_DLH) = (divisor >> 8U) & 0xFFU;

    REG32(base_addr, UART_LCR) = UART_LCR_8N1;       /* DLAB=0, 8 data bits, 1 stop, no parity */
    REG32(base_addr, UART_FCR) = UART_FCR_FIFO_EN;
    REG32(base_addr, UART_IER) = 0x00U;               /* polling mode: no interrupts */

    REG32(base_addr, UART_MDR1) = UART_MDR1_UART16X; /* enable */
}

void xuart_putc(uint32_t base_addr, char c)
{
    uint32_t timeout = 100000U;
    while (!(REG32(base_addr, UART_LSR) & UART_LSR_THRE) && (timeout-- != 0U))
        ;
    REG32(base_addr, UART_THR) = (uint32_t)c;
}

void xuart_puts(uint32_t base_addr, const char *s)
{
    while (*s)
    {
        if (*s == '\n')
            xuart_putc(base_addr, '\r');
        xuart_putc(base_addr, *s++);
    }
}
