#include "rtos_tasks.h"
#include <stddef.h>
#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_port_ch32h417.h"
#include "xrtos_return.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "uart_console.h"
#include "blinky.h"
#include "test_peripherals.h"
#include "usb_dev.h"

#define XSDK_CH32H417_TICK_HZ            100U
#define XSDK_CH32H417_TASK_A_ID          0U
#define XSDK_CH32H417_TASK_B_ID          1U
#define XSDK_CH32H417_TASK_A_PRIORITY    5U
#define XSDK_CH32H417_TASK_B_PRIORITY    5U
#define XSDK_CH32H417_TASK_A_DELAY_TICKS 10U
#define XSDK_CH32H417_TASK_B_DELAY_TICKS 10U
#define XSDK_CH32H417_TASK_STACK_WORDS   128U

static volatile uint32_t xSDK_CH32H417_Heartbeat_Tick;

static xRTOS_Kernel_Context_t xSDK_CH32H417_Kernel;
static xRTOS_Task_Context_t xSDK_CH32H417_Idle_Task;
static xRTOS_Task_Context_t xSDK_CH32H417_Task_A;
// static xRTOS_Task_Context_t xSDK_CH32H417_Task_B;
static uint32_t xSDK_CH32H417_Idle_Stack[XSDK_CH32H417_TASK_STACK_WORDS];
static uint32_t xSDK_CH32H417_Task_A_Stack[XSDK_CH32H417_TASK_STACK_WORDS];
// static uint32_t xSDK_CH32H417_Task_B_Stack[XSDK_CH32H417_TASK_STACK_WORDS];

static void xSDK_CH32H417_Idle_Entry(void *arg)
{
    (void)arg;
    for (;;)
    {
        __asm volatile("wfi");
    }
}

static void xSDK_CH32H417_Task_A_Entry(void *arg)
{
    (void)arg;
    static bool state = false;
    static uint32_t usb_dump_divider = 0U;

    for (;;)
    {
        state = !state;
        xSDK_Blinky_Set_Blue(state);

        usb_dump_divider++;
        if (usb_dump_divider >= 50U)
        {
            usb_dump_divider = 0U;
            xSDK_USB_Dump_Status("tick");
        }
        (void)xRTOS_Task_Delay(XSDK_CH32H417_TASK_A_DELAY_TICKS);
    }
}

// static void xSDK_CH32H417_Task_B_Entry(void *arg)
// {
//     (void)arg;
//     static bool state = false;

//     for (;;)
//     {
//         state = !state;
//         xSDK_Blinky_Set_Green(state);

//         // ---- 1. Timer Test ----
//         xSDK_Timer_Test_Run();

//         // ---- 2. I2C Test ----
//         xSDK_I2C_Test_Run();

//         // ---- 3. SPI Test ----
//         xSDK_SPI_Test_Run();

//         (void)xRTOS_Task_Delay(XSDK_CH32H417_TASK_B_DELAY_TICKS);
//     }
// }

static void xSDK_CH32H417_Create_Task(xRTOS_Task_Context_t *task_ctx,
                                      uint32_t task_id,
                                      uint32_t priority,
                                      xRTOS_Task_Entry_t entry,
                                      uint32_t *stack_mem,
                                      const char *name)
{
    xRTOS_Task_Config_t task_config = {
        .task_id = task_id,
        .priority = priority,
        .entry = entry,
        .entry_arg = NULL,
        .stack_mem = stack_mem,
        .stack_words = XSDK_CH32H417_TASK_STACK_WORDS,
        .name = name,
    };

    if (xRTOS_Task_Create(task_ctx, &task_config) != xRETURN_xRTOS_OK)
    {
        xSDK_Console_Write("xRTOS task create failed\r\n");
        for (;;)
        {
        }
    }
}

void xSDK_RTOS_Init_And_Start(uint32_t hclk_hz)
{
    if (xRTOS_Kernel_Init(&xSDK_CH32H417_Kernel, &xrtos_ch32h417_port_ops) != xRETURN_xRTOS_OK)
    {
        xSDK_Console_Write("xRTOS init failed\r\n");
        for (;;)
        {
        }
    }

    xSDK_CH32H417_Create_Task(&xSDK_CH32H417_Idle_Task, xRTOS_IDLE_TASK_ID, xRTOS_IDLE_PRIORITY, xSDK_CH32H417_Idle_Entry,
                              xSDK_CH32H417_Idle_Stack, "idle");
    xSDK_CH32H417_Create_Task(&xSDK_CH32H417_Task_A, XSDK_CH32H417_TASK_A_ID, XSDK_CH32H417_TASK_A_PRIORITY, xSDK_CH32H417_Task_A_Entry,
                              xSDK_CH32H417_Task_A_Stack, "task_a");
    // xSDK_CH32H417_Create_Task(&xSDK_CH32H417_Task_B, XSDK_CH32H417_TASK_B_ID, XSDK_CH32H417_TASK_B_PRIORITY, xSDK_CH32H417_Task_B_Entry,
    //                           xSDK_CH32H417_Task_B_Stack, "task_b");

    xSDK_Console_Write("xRTOS tasks registered\r\n");

    xRTOS_Port_CH32H417_Timer_Init(hclk_hz, XSDK_CH32H417_TICK_HZ);

    if (xRTOS_Kernel_Start() != xRETURN_xRTOS_OK)
    {
        xSDK_Console_Write("xRTOS scheduler start failed\r\n");
        for (;;)
        {
        }
    }

    uint32_t observed_tick = 0U;
    for (;;)
    {
        uint32_t current_tick = xRTOS_Tick_Get();

        if ((current_tick - observed_tick) >= XSDK_CH32H417_TICK_HZ)
        {
            observed_tick += XSDK_CH32H417_TICK_HZ;
            xSDK_CH32H417_Heartbeat_Tick = observed_tick;
            xSDK_Console_PutChar('.');
        }
    }
}
