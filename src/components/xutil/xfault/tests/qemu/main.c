#include <stdint.h>
#include <stdbool.h>

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_port_arm_r5.h"

#include "xfault.h"
#include "xreturn.h"

#include "uart.h"

// ---- linker symbols for xFAULT stack bounds --------------------------------
extern uint32_t _stack_bottom;
extern uint32_t _sys_stack_top;

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

// ---- xFAULT UART output ----------------------------------------------------

static xRETURN_t fault_uart_write(void *ctx, const uint8_t *buf, size_t len, size_t *written)
{
    (void)ctx;
    for (uint32_t i = 0U; i < len; i++)
    {
        uart_putc((char)buf[i]);
    }
    *written = len;
    return xRETURN_OK;
}

static xRETURN_t fault_uart_flush(void *ctx)
{
    (void)ctx;
    return xRETURN_OK;
}

static const xFAULT_Output_t s_fault_output = {.write = fault_uart_write, .flush = fault_uart_flush};

// ---- Phase 1: xFAULT -------------------------------------------------------

static uint32_t s_test_stack[512];
static xFAULT_Context_t s_fault_ctx;

static void phase1_xfault(void)
{
    uart_puts("--- Phase 1: xFAULT ---\n");

    CHECK_OK("xFAULT_Context_Init", xFAULT_Context_Init(&s_fault_ctx));
    CHECK_TRUE("is_valid = false after Init", !s_fault_ctx.is_valid);

    // Read real CP15 fault status registers (works on ARM target)
    xRETURN_t cp15_ret = xFAULT_Capture_CP15(&s_fault_ctx.cp15);
    CHECK_TRUE("xFAULT_Capture_CP15 OK or UNSUPPORTED", (cp15_ret == xRETURN_OK) || (cp15_ret == xRETURN_xERR_xFAULT_UNSUPPORTED_TARGET));

    // Read current frame pointer for backtrace walk
    xFAULT_Address_t fp_val = 0U;
#if defined(__arm__) && !defined(__thumb__)
    __asm volatile("mov %0, fp" : "=r"(fp_val));
#endif
    s_fault_ctx.core.fp = fp_val;

    xFAULT_Address_t stack_base = (xFAULT_Address_t)(uintptr_t)(s_test_stack + 512U);
    xFAULT_Address_t stack_limit = (xFAULT_Address_t)(uintptr_t)s_test_stack;

    xRETURN_t bt_ret = xFAULT_Backtrace_Capture(&s_fault_ctx, stack_base, stack_limit);
    CHECK_TRUE("xFAULT_Backtrace_Capture OK", bt_ret == xRETURN_OK);
    CHECK_TRUE("is_valid = true after Backtrace", s_fault_ctx.is_valid);

    uart_puts("  xFAULT dump:\n");
    xFAULT_Config_t dump_cfg;
    dump_cfg.output = &s_fault_output;
    dump_cfg.output_ctx = NULL;
    dump_cfg.halt = NULL;
    dump_cfg.halt_ctx = NULL;
    CHECK_OK("xFAULT_Dump_Text", xFAULT_Dump_Text(&s_fault_ctx, &dump_cfg));

    // Configure global fatal handler (does not trigger a fault)
    CHECK_OK("xFAULT_Fatal_Config_Set", xFAULT_Fatal_Config_Set(&dump_cfg, stack_base, stack_limit));
}

// ---- xRTOS task + kernel ---------------------------------------------------

static xRTOS_Kernel_Context_t s_kernel;
static xRTOS_Task_Context_t s_idle_ctx;
static uint32_t s_idle_stack[128];
static xRTOS_Task_Context_t s_test_ctx;

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

    uart_puts("\n--- xFAULT Cortex-R5 QEMU Smoke Test ---\n\n");

    s_pass = 0U;
    s_fail = 0U;

    phase1_xfault();

    uart_puts("\n=== xFAULT Results: PASS=");
    uart_puti(s_pass);
    uart_puts(" FAIL=");
    uart_puti(s_fail);
    uart_puts(" ===\n");

    if (s_fail == 0U)
    {
        uart_puts("ALL XFAULT TESTS PASSED SUCCESSFULLY!\n");
    }
    else
    {
        uart_puts("SOME XFAULT TESTS FAILED.\n");
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
