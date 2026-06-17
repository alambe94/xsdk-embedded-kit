// Copyright 2022 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file main.c
// @brief AM243x hardware 32-slot round-robin test.
//
// Task / priority layout (all 32 task-ID slots occupied)
// -------------------------------------------------------------
//  task_id   priority  role
//     0         0      supervisor
//   1..29       2      workers (29-task round-robin group)
//    30         1      timer daemon (auto-created by Kernel_Init)
//    31        31      idle
// -------------------------------------------------------------
//
// The supervisor delays 100 ticks while 29 workers count iterations
// at the same priority. On wake it verifies every counter is non-zero
// and max < 2xmin (fair round-robin), then writes g_rr32_done:
//   1 = PASS, 2 = FAIL.
//
// Connect a UART-USB cable to UART0 (115200 8N1) to see live output.
// Attach a debugger (OpenOCD + GDB) and watch g_rr32_done for the
// automated pass/fail result - see tools/gdb/rr32_hw_test.gdb.

#include <stdbool.h>
#include <stdint.h>

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_port_arm_r5.h"
#include "xrtos_port_am243x.h"
#include "xsdk_soc_mmr.h"
#include "xuart.h"
#include "xuart_drv.h"
#include "xtimer.h"

// -- Board constants --------------------------------------------------------

#define UART0_BASE   0x02800000U
#define UART0_CLK_HZ 25000000U
#define UART0_BAUD   115200U

#define TIMER8_BASE    0x02480000U
#define TIMER8_IRQ     160U
#define TIMER8_CLK_HZ  25000000U
#define TICK_PERIOD_US 10000U // 10 ms -> 100 Hz tick

#define TIMER8_CLK_SRC_MUX_ADDR  (0x430081D0UL)
#define TIMER8_CLK_SRC_HFOSC0    (0x0UL)
#define TIMER8_CLK_MUX_PARTITION (2U)

// -- Test configuration -----------------------------------------------------

#define RR32_WORKER_COUNT 29U // task IDs 1-29

// GDB-readable result: 0=running, 1=PASS, 2=FAIL.
volatile uint32_t g_rr32_done = 0U;

// -- Kernel and task storage ------------------------------------------------

static xRTOS_Kernel_Context_t s_kernel;
static xRTOS_Task_Context_t s_supervisor_ctx;
static xRTOS_Task_Context_t s_worker_ctx[RR32_WORKER_COUNT];
static xRTOS_Task_Context_t s_idle_ctx;

static uint32_t s_supervisor_stack[512];
static uint32_t s_worker_stack[RR32_WORKER_COUNT][128];
static uint32_t s_idle_stack[128];

// -- Round-robin worker state -----------------------------------------------

static volatile uint32_t s_worker_count[RR32_WORKER_COUNT];
static volatile bool s_workers_stop;

static xUART_Context_t s_uart_ctx;
static xUART_AM243x_Context_t s_am243x_uart_ctx;

// -- Helper functions -------------------------------------------------------

static void uart_write(const char *s)
{
    if (s == NULL)
    {
        return;
    }
    size_t len = 0;
    while (s[len] != '\0')
    {
        len++;
    }
    if (len > 0)
    {
        (void)xUART_Transmit(&s_uart_ctx, (const uint8_t *)s, (uint32_t)len, 1000U);
    }
}

// -- ISR -------------------------------------------------------------------

static void timer_isr(void *args)
{
    (void)args;
    xTIMER_Clear_IRQ(TIMER8_BASE);
    xRTOS_Port_AM243x_Tick_ISR(NULL);
}

// -- Task implementations ---------------------------------------------------

static void worker_entry(void *arg)
{
    volatile uint32_t *count = (volatile uint32_t *)arg;
    while (!s_workers_stop)
    {
        (*count)++;
    }
    xRTOS_Task_Exit();
}

static void supervisor_entry(void *arg)
{
    (void)arg;

    uart_write("\nxRTOS 32-Slot Round-Robin Test (AM243x)\n");
    uart_write("Workers 1-29 at priority 2. Sleeping 100 ticks...\n");

    (void)xRTOS_Task_Delay(100U);

    s_workers_stop = true;
    (void)xRTOS_Task_Delay(8U);

    uint32_t min_c = 0xFFFFFFFFU;
    uint32_t max_c = 0U;
    bool any_zero = false;

    for (uint32_t i = 0U; i < RR32_WORKER_COUNT; i++)
    {
        uint32_t c = s_worker_count[i];
        if (c == 0U)
        {
            any_zero = true;
        }
        if (c < min_c)
        {
            min_c = c;
        }
        if (c > max_c)
        {
            max_c = c;
        }
    }

    const bool pass = !any_zero && (max_c < (min_c * 2U));

    if (pass)
    {
        uart_write("RR32 PASS: 29-task round-robin balanced\n");
        g_rr32_done = 1U;
    }
    else
    {
        uart_write("RR32 FAIL: worker counts not balanced\n");
        g_rr32_done = 2U;
    }

    for (;;)
    {
        // Hold so GDB watchpoint on g_rr32_done can be read after firing.
    }
}

static void idle_entry(void *arg)
{
    (void)arg;
    for (;;)
    {
    }
}

// -- main ------------------------------------------------------------------

int main(void)
{
    s_am243x_uart_ctx.base_addr = UART0_BASE;
    s_am243x_uart_ctx.input_clock_hz = UART0_CLK_HZ;

    xUART_Config_t uart_cfg = {.baud_rate = UART0_BAUD,
                               .data_bits = xUART_DATA_BITS_8,
                               .stop_bits = xUART_STOP_BITS_1,
                               .parity = xUART_PARITY_NONE,
                               .flow_control = xUART_FLOW_CONTROL_NONE,
                               .callbacks.on_event = NULL};

    if (xUART_Init(&s_uart_ctx, &uart_cfg) != xRETURN_OK)
    {
        for (;;)
            ;
    }

    xUART_Start_Config_t uart_start_cfg = {.port = 0U, .drv_ops = &xUART_AM243x_Driver_Ops, .drv_ctx = &s_am243x_uart_ctx};

    if (xUART_Start(&s_uart_ctx, &uart_start_cfg) != xRETURN_OK)
    {
        for (;;)
            ;
    }

    uart_write("\nxSDK AM243x RR32 boot\n");

    xRTOS_Port_AM243x_Init();

    xsdk_soc_mmr_unlock_main(TIMER8_CLK_MUX_PARTITION);
    *(volatile uint32_t *)TIMER8_CLK_SRC_MUX_ADDR = TIMER8_CLK_SRC_HFOSC0;
    xsdk_soc_mmr_lock_main(TIMER8_CLK_MUX_PARTITION);

    xTIMER_Init_Periodic(TIMER8_BASE, TICK_PERIOD_US, TIMER8_CLK_HZ);
    xRTOS_Port_AM243x_Register_IRQ(TIMER8_IRQ, timer_isr, NULL, 15U, false);
    xRTOS_Port_AM243x_Enable_IRQ(TIMER8_IRQ);
    xTIMER_Start(TIMER8_BASE);

    xRTOS_Kernel_Init(&s_kernel, &xrtos_arm_r5_port_ops);

    xRTOS_Task_Create(&s_idle_ctx, &(xRTOS_Task_Config_t){.task_id = xRTOS_IDLE_TASK_ID,
                                                          .priority = xRTOS_IDLE_PRIORITY,
                                                          .entry = idle_entry,
                                                          .entry_arg = NULL,
                                                          .stack_mem = s_idle_stack,
                                                          .stack_words = 128U});

    xRTOS_Task_Create(&s_supervisor_ctx, &(xRTOS_Task_Config_t){.task_id = 0U,
                                                                .priority = 0U,
                                                                .entry = supervisor_entry,
                                                                .entry_arg = NULL,
                                                                .stack_mem = s_supervisor_stack,
                                                                .stack_words = 512U});

    for (uint32_t i = 0U; i < RR32_WORKER_COUNT; i++)
    {
        xRTOS_Task_Create(&s_worker_ctx[i], &(xRTOS_Task_Config_t){.task_id = 1U + i,
                                                                   .priority = 2U,
                                                                   .entry = worker_entry,
                                                                   .entry_arg = (void *)&s_worker_count[i],
                                                                   .stack_mem = s_worker_stack[i],
                                                                   .stack_words = 128U});
    }

    uart_write("Starting scheduler...\n");
    xRTOS_Kernel_Start();

    return 0;
}

// EOF /////////////////////////////////////////////////////////////////////////////
