#include <stddef.h>
#include <stdint.h>

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_port_arm_r5.h"
#include "xrtos_port_am243x.h"
#include "xsdk_soc_mmr.h"
#include "xuart.h"
#include "xtimer.h"

#define UART0_BASE 0x02800000U
/* UART0 functional clock source:
 *   25 000 000 Hz - HFOSC0 oscillator (no SBL/TIFS: JTAG bare-metal load)
 *   48 000 000 Hz - MAIN_PLL0 HSDIV4_CLKOUT3 (when SBL/TIFS configures PLL) */
#define UART0_CLK_HZ    25000000U
#define UART0_BAUD      115200U
#define TIMER8_BASE     0x02480000U
#define TIMER8_IRQ      160U
#define TIMER8_CLK_HZ   25000000U
#define TICK_PERIOD_US  10000U /* 10 ms */
#define HEARTBEAT_TICKS 100U   /* 100 ticks = 1 s */

/* TIMER8 clock source mux - MAIN_CTRL_MMR0 partition 2 (0x43008000-0x4300BFFF) */
#define TIMER8_CLK_SRC_MUX_ADDR  (0x430081D0UL)
#define TIMER8_CLK_SRC_HFOSC0    (0x0UL) /* 25 MHz MCU_HFOSC0 */
#define TIMER8_CLK_MUX_PARTITION (2U)

#define TASK_A_ID   0U
#define TASK_B_ID   1U
#define STACK_WORDS 128U

static uint32_t task_a_stack[STACK_WORDS];
static uint32_t task_b_stack[STACK_WORDS];
static uint32_t idle_stack[STACK_WORDS];

static xRTOS_Kernel_Context_t kernel_ctx;
static xRTOS_Task_Context_t task_a_ctx;
static xRTOS_Task_Context_t task_b_ctx;
static xRTOS_Task_Context_t idle_ctx;

static void timer_isr(void *args)
{
    (void)args;
    xtimer_clear_irq(TIMER8_BASE);
    xRTOS_Port_AM243x_Tick_ISR(NULL);
}

static void task_a_entry(void *arg)
{
    (void)arg;
    for (;;)
    {
        xuart_puts(UART0_BASE, "task A heartbeat\n");
        xRTOS_Task_Delay(HEARTBEAT_TICKS);
    }
}

static void task_b_entry(void *arg)
{
    (void)arg;
    for (;;)
    {
        xuart_puts(UART0_BASE, "task B heartbeat\n");
        xRTOS_Task_Delay(HEARTBEAT_TICKS);
    }
}

static void idle_entry(void *arg)
{
    (void)arg;
    for (;;)
        ;
}

int main(void)
{
    xuart_init(UART0_BASE, UART0_BAUD, UART0_CLK_HZ);
    xuart_puts(UART0_BASE, "\nxSDK AM243x boot\n");

    xRTOS_Port_AM243x_Init();

    /* Route TIMER8 clock to MCU_HFOSC0 (25 MHz). The mux register sits in
     * MAIN_CTRL_MMR0 partition 2 which requires KICK unlock before write. */
    xsdk_soc_mmr_unlock_main(TIMER8_CLK_MUX_PARTITION);
    *(volatile uint32_t *)TIMER8_CLK_SRC_MUX_ADDR = TIMER8_CLK_SRC_HFOSC0;
    xsdk_soc_mmr_lock_main(TIMER8_CLK_MUX_PARTITION);

    xtimer_init_periodic(TIMER8_BASE, TICK_PERIOD_US, TIMER8_CLK_HZ);
    xRTOS_Port_AM243x_Register_IRQ(TIMER8_IRQ, timer_isr, NULL, 15U, false);
    xRTOS_Port_AM243x_Enable_IRQ(TIMER8_IRQ);
    xtimer_start(TIMER8_BASE);

    xRTOS_Kernel_Init(&kernel_ctx, &xrtos_arm_r5_port_ops);

    xRTOS_Task_Config_t cfg;

    cfg = (xRTOS_Task_Config_t){.task_id = TASK_A_ID,
                                .priority = 1U,
                                .entry = task_a_entry,
                                .entry_arg = NULL,
                                .stack_mem = task_a_stack,
                                .stack_words = STACK_WORDS};
    xRTOS_Task_Create(&task_a_ctx, &cfg);

    cfg = (xRTOS_Task_Config_t){.task_id = TASK_B_ID,
                                .priority = 1U,
                                .entry = task_b_entry,
                                .entry_arg = NULL,
                                .stack_mem = task_b_stack,
                                .stack_words = STACK_WORDS};
    xRTOS_Task_Create(&task_b_ctx, &cfg);

    cfg = (xRTOS_Task_Config_t){.task_id = xRTOS_IDLE_TASK_ID,
                                .priority = xRTOS_IDLE_PRIORITY,
                                .entry = idle_entry,
                                .entry_arg = NULL,
                                .stack_mem = idle_stack,
                                .stack_words = STACK_WORDS};
    xRTOS_Task_Create(&idle_ctx, &cfg);

    xuart_puts(UART0_BASE, "starting scheduler\n");

    xRTOS_Kernel_Start(); /* does not return */

    return 0;
}
