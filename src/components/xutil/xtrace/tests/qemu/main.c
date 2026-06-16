#include <stdint.h>
#include <stdbool.h>

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_port_arm_r5.h"

#include "xtrace.h"
#include "xreturn.h"

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

// ---- xTRACE UART transport -------------------------------------------------

// Writes each byte as two hex digits so binary records appear readable.
static xRETURN_t trace_uart_write(void *ctx, const uint8_t *buf, size_t len, size_t *written)
{
    static const char hex[] = "0123456789ABCDEF";
    (void)ctx;
    for (uint32_t i = 0U; i < len; i++)
    {
        uart_putc(hex[(buf[i] >> 4U) & 0xFU]);
        uart_putc(hex[buf[i] & 0xFU]);
        uart_putc(' ');
    }
    *written = len;
    return xRETURN_OK;
}

static const xTRACE_Transport_t s_trace_transport = {.write = trace_uart_write};

// ---- xTRACE tick emulator --------------------------------------------------

static uint32_t s_tick;

static xTRACE_Time_t tick_fn(void *ctx)
{
    (void)ctx;
    return s_tick++;
}

// ---- Phase 1: xTRACE -------------------------------------------------------

#define TRACE_CAPACITY 8U

static uint8_t s_trace_buf[TRACE_CAPACITY * 12U];
static xTRACE_Context_t s_trace_ctx;

static void phase1_xtrace(void)
{
    uart_puts("--- Phase 1: xTRACE ---\n");

    xTRACE_Config_t cfg;
    cfg.buffer = s_trace_buf;
    cfg.capacity_bytes = TRACE_CAPACITY * 12U; // 12 bytes/record estimated
    cfg.timestamp_fn = tick_fn;
    cfg.timestamp_ctx = NULL;
    cfg.timestamp_hz = 1000U;
    cfg.overrun_policy = xTRACE_OVERRUN_DROP;
    cfg.is_enabled = true;

    CHECK_OK("xTRACE_Init", xTRACE_Init(&s_trace_ctx, &cfg, &s_trace_transport, NULL));

    // Emit 5 events
    // user event: ID 0x60 (first user slot per xrtos_trace.h namespace)
    xTRACE_Emit1(&s_trace_ctx, 0x60U, 0x11U);
    xTRACE_Emit1(&s_trace_ctx, 0x60U, 0x22U);
    xTRACE_Emit1(&s_trace_ctx, 0x60U, 0x33U);
    xTRACE_Emit1(&s_trace_ctx, 0x60U, 0x44U);
    xTRACE_Emit1(&s_trace_ctx, 0x60U, 0x55U);

    xTRACE_Status_t st;
    CHECK_OK("xTRACE_Get_Status", xTRACE_Get_Status(&s_trace_ctx, &st));
    CHECK_TRUE("5 records buffered", st.used_bytes > 0U);
    CHECK_TRUE("0 dropped before overflow", st.dropped_count == 0U);

    uart_puts("  Records (hex): ");
    CHECK_OK("xTRACE_Flush", xTRACE_Flush(&s_trace_ctx));
    uart_puts("\n");

    CHECK_OK("xTRACE_Get_Status after flush", xTRACE_Get_Status(&s_trace_ctx, &st));
    CHECK_TRUE("0 records after flush", st.used_bytes == 0U);

    // Overflow: write enough events to exceed capacity_bytes (96 bytes)
    for (uint32_t i = 0U; i < 50U; i++)
    {
        xTRACE_Emit1(&s_trace_ctx, 0x60U, i);
    }
    CHECK_OK("xTRACE_Get_Status overflow", xTRACE_Get_Status(&s_trace_ctx, &st));
    CHECK_TRUE("dropped_count > 0 on overflow", st.dropped_count > 0U);

    CHECK_OK("xTRACE_Deinit", xTRACE_Deinit(&s_trace_ctx));
}

// ---- xRTOS task + kernel ---------------------------------------------------

static xRTOS_Kernel_Context_t s_kernel;
static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[128];
static xRTOS_Task_Context_t s_test_ctx;
static uint32_t s_test_stack[512];

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

    uart_puts("\n--- xTRACE Cortex-R5 QEMU Smoke Test ---\n\n");

    s_pass = 0U;
    s_fail = 0U;
    s_tick = 0U;

    phase1_xtrace();

    uart_puts("\n=== xTRACE Results: PASS=");
    uart_puti(s_pass);
    uart_puts(" FAIL=");
    uart_puti(s_fail);
    uart_puts(" ===\n");

    if (s_fail == 0U)
    {
        uart_puts("ALL XTRACE TESTS PASSED SUCCESSFULLY!\n");
    }
    else
    {
        uart_puts("SOME XTRACE TESTS FAILED.\n");
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
        .task_id = 1U, .priority = 2U, .entry = test_entry, .entry_arg = NULL, .stack_mem = s_test_stack, .stack_words = 512U};
    xRTOS_Task_Create(&s_test_ctx, &test_cfg);

    xRTOS_Kernel_Start();

    return 0;
}
