#include "xtimer.h"

/* DMTimer register offsets (Keystone3 / AM64x / AM243x) */
#define TIOCP_CFG  0x10U
#define TISR       0x28U
#define TIER       0x2CU
#define TCLR       0x38U
#define TCRR       0x3CU
#define TLDR       0x40U

#define TCLR_ST    (1U << 0)
#define TCLR_AR    (1U << 1)
#define TIER_OVF   (1U << 1)
#define TISR_OVF   (1U << 1)

#define REG32(base, off) (*(volatile uint32_t *)((base) + (off)))

void xtimer_init_periodic(uint32_t base_addr,
                           uint32_t period_us,
                           uint32_t module_clk_hz)
{
    uint32_t ticks = (uint32_t)((uint64_t)module_clk_hz * period_us / 1000000U);
    uint32_t load  = 0xFFFFFFFFU - ticks + 1U;

    REG32(base_addr, TIOCP_CFG) = 0x1U;
    while (REG32(base_addr, TIOCP_CFG) & 0x1U)
        ;

    REG32(base_addr, TCLR) = 0U;
    REG32(base_addr, TLDR) = load;
    REG32(base_addr, TCRR) = load;
    REG32(base_addr, TISR) = TISR_OVF;
    REG32(base_addr, TIER) = TIER_OVF;
}

void xtimer_start(uint32_t base_addr)
{
    REG32(base_addr, TCLR) = TCLR_ST | TCLR_AR;
}

void xtimer_stop(uint32_t base_addr)
{
    REG32(base_addr, TCLR) &= ~TCLR_ST;
}

void xtimer_clear_irq(uint32_t base_addr)
{
    REG32(base_addr, TISR) = TISR_OVF;
}
