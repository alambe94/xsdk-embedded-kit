#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_port_arm_r5.h"

#include "xusbh_core.h"
#include "xusbh_hcd.h"
#include "xusbh_return.h"

#include "uart.h"

// ---- pass/fail helpers -----------------------------------------------------

static uint32_t s_pass;
static uint32_t s_fail;

#define CHECK_OK(label, expr)                                                                                                              \
    do                                                                                                                                     \
    {                                                                                                                                      \
        xRETURN_t _r = (expr);                                                                                                             \
        if (_r == xRETURN_OK)                                                                                                              \
        {                                                                                                                                  \
            uart_puts("  PASS: " label "\n");                                                                                              \
            s_pass++;                                                                                                                      \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            uart_puts("  FAIL: " label " (err=0x");                                                                                        \
            uart_puti(_r);                                                                                                                 \
            uart_puts(")\n");                                                                                                              \
            s_fail++;                                                                                                                      \
        }                                                                                                                                  \
    } while (0)

#define CHECK_TRUE(label, expr)                                                                                                            \
    do                                                                                                                                     \
    {                                                                                                                                      \
        bool _v = (expr);                                                                                                                  \
        if (_v)                                                                                                                            \
        {                                                                                                                                  \
            uart_puts("  PASS: " label "\n");                                                                                              \
            s_pass++;                                                                                                                      \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            uart_puts("  FAIL: " label "\n");                                                                                              \
            s_fail++;                                                                                                                      \
        }                                                                                                                                  \
    } while (0)

// ---- mock HCD implementation -----------------------------------------------

static xRETURN_t mock_hcd_init(void *hcd_ctx, void *host_ctx, xUSBH_HCD_Event_Callback_t callback)
{
    (void)hcd_ctx;
    (void)host_ctx;
    (void)callback;
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_deinit(void *hcd_ctx)
{
    (void)hcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_start(void *hcd_ctx)
{
    (void)hcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_stop(void *hcd_ctx)
{
    (void)hcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_enable_interrupts(void *hcd_ctx)
{
    (void)hcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_disable_interrupts(void *hcd_ctx)
{
    (void)hcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_port_power(void *hcd_ctx, uint8_t port, bool enable)
{
    (void)hcd_ctx;
    (void)port;
    (void)enable;
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_port_reset(void *hcd_ctx, uint8_t port)
{
    (void)hcd_ctx;
    (void)port;
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_get_port_status(void *hcd_ctx, uint8_t port, xUSBH_HCD_Port_Status_t *status)
{
    (void)hcd_ctx;
    (void)port;
    if (status != NULL)
    {
        status->is_connected = false;
        status->is_enabled = false;
        status->is_suspended = false;
        status->is_overcurrent = false;
        status->speed = USB_SPEED_HIGH;
    }
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_submit_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer)
{
    (void)hcd_ctx;
    (void)transfer;
    return xRETURN_OK;
}

static xRETURN_t mock_hcd_cancel_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer)
{
    (void)hcd_ctx;
    (void)transfer;
    return xRETURN_OK;
}

static uint32_t mock_hcd_get_frame_number(void *hcd_ctx)
{
    (void)hcd_ctx;
    return 0U;
}

static xUSBH_HCD_Ops_t s_mock_hcd_ops = {
    .init = mock_hcd_init,
    .deinit = mock_hcd_deinit,
    .start = mock_hcd_start,
    .stop = mock_hcd_stop,
    .enable_interrupts = mock_hcd_enable_interrupts,
    .disable_interrupts = mock_hcd_disable_interrupts,
    .port_power = mock_hcd_port_power,
    .port_reset = mock_hcd_port_reset,
    .get_port_status = mock_hcd_get_port_status,
    .submit_transfer = mock_hcd_submit_transfer,
    .cancel_transfer = mock_hcd_cancel_transfer,
    .get_frame_number = mock_hcd_get_frame_number,
};

// ---- xRTOS task + kernel ---------------------------------------------------

static xRTOS_Kernel_Context_t s_kernel;
static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[128];
static xRTOS_Task_Context_t s_test_ctx;
static uint32_t s_test_stack[1024];

static void idle_entry(void *arg)
{
    (void)arg;
    for (;;)
    {
    }
}

static void test_entry(void *arg)
{
    (void)arg;

    uart_puts("\n--- xUSBH Cortex-R5 QEMU Smoke Test ---\n\n");

    s_pass = 0U;
    s_fail = 0U;

    static xUSBH_Context_t host_ctx;

    xUSBH_Init_Config_t init_cfg = {
        .root_port_count = 1U,
    };
    CHECK_OK("xUSBH_Init", xUSBH_Init(&host_ctx, &init_cfg));

    static int dummy_hcd_ctx = 0;
    xUSBH_Start_Config_t start_cfg = {
        .hcd_ops = &s_mock_hcd_ops,
        .hcd_ctx = &dummy_hcd_ctx,
    };
    CHECK_OK("xUSBH_Start", xUSBH_Start(&host_ctx, &start_cfg));

    bool started = false;
    CHECK_OK("xUSBH_Is_Started", xUSBH_Is_Started(&host_ctx, &started));
    CHECK_TRUE("started == true", started);

    CHECK_OK("xUSBH_Stop", xUSBH_Stop(&host_ctx));
    CHECK_OK("xUSBH_Is_Started (stopped)", xUSBH_Is_Started(&host_ctx, &started));
    CHECK_TRUE("started == false", !started);

    uart_puts("\n=== xUSBH Results: PASS=");
    uart_puti(s_pass);
    uart_puts(" FAIL=");
    uart_puti(s_fail);
    uart_puts(" ===\n");

    if (s_fail == 0U)
    {
        uart_puts("ALL XUSBH TESTS PASSED SUCCESSFULLY!\n");
    }
    else
    {
        uart_puts("SOME XUSBH TESTS FAILED.\n");
    }

    for (;;)
    {
    }
}

int main(void)
{
    uart_init();

    xRTOS_Kernel_Init(&s_kernel, &xrtos_arm_r5_port_ops);

    xRTOS_Task_Config_t idle_cfg = {.task_id = xRTOS_IDLE_TASK_ID,
                                    .priority = xRTOS_IDLE_PRIORITY,
                                    .entry = idle_entry,
                                    .entry_arg = NULL,
                                    .stack_mem = s_idle_stack,
                                    .stack_words = 128U};
    xRTOS_Task_Create(&s_idle_ctx, &idle_cfg);

    xRTOS_Task_Config_t test_cfg = {
        .task_id = 1U, .priority = 2U, .entry = test_entry, .entry_arg = NULL, .stack_mem = s_test_stack, .stack_words = 1024U};
    xRTOS_Task_Create(&s_test_ctx, &test_cfg);

    xRTOS_Kernel_Start();

    return 0;
}