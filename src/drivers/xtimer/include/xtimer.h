#ifndef XTIMER_H
#define XTIMER_H

#include <stdint.h>

/* Configure DMTimer for periodic overflow interrupts.
 * period_us: desired period in microseconds
 * module_clk_hz: timer input clock in Hz (e.g. 25000000 for 25 MHz)
 * Clocks and pinmux assumed enabled by SBL Null. */
void xtimer_init_periodic(uint32_t base_addr, uint32_t period_us, uint32_t module_clk_hz);

void xtimer_start(uint32_t base_addr);
void xtimer_stop(uint32_t base_addr);
void xtimer_clear_irq(uint32_t base_addr);

#endif
