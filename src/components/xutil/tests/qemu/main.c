#include <stdint.h>
#include <stdbool.h>

// Overrides must be defined before ANY SDK header is included,
// as they transitively include xassert.h.
static void test_assert_handler(const char *file, int line, const char *expr, const char *msg);
#define xASSERT_HANDLER(file, line, expr, msg) test_assert_handler((file), (line), (expr), (msg))
#define xASSERT_HOOK()                         ((void)0) // prevent system halt
#ifdef xSDK_ENABLE_ASSERT
#undef xSDK_ENABLE_ASSERT
#endif
#define xSDK_ENABLE_ASSERT 1

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_port_arm_r5.h"
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

#define CHECK_EQUAL(label, expected, actual)                                                                                               \
    do                                                                                                                                     \
    {                                                                                                                                      \
        uint32_t _exp = (uint32_t)(expected);                                                                                              \
        uint32_t _act = (uint32_t)(actual);                                                                                                \
        if (_exp == _act)                                                                                                                  \
        {                                                                                                                                  \
            uart_puts("  PASS: " label "\n");                                                                                              \
            s_pass++;                                                                                                                      \
        }                                                                                                                                  \
        else                                                                                                                               \
        {                                                                                                                                  \
            uart_puts("  FAIL: " label " (expected=");                                                                                     \
            uart_puti(_exp);                                                                                                               \
            uart_puts(", actual=");                                                                                                        \
            uart_puti(_act);                                                                                                               \
            uart_puts(")\n");                                                                                                              \
            s_fail++;                                                                                                                      \
        }                                                                                                                                  \
    } while (0)

// ---- xASSERT setup ---------------------------------------------------------

static uint32_t s_assert_count;
static const char *s_assert_file;
static int s_assert_line;
static const char *s_assert_expr;
static const char *s_assert_msg;

static void test_assert_handler(const char *file, int line, const char *expr, const char *msg)
{
    s_assert_count++;
    s_assert_file = file;
    s_assert_line = line;
    s_assert_expr = expr;
    s_assert_msg = msg;
}

#include "xassert.h"

// ---- xLOG setup ------------------------------------------------------------

static uint32_t s_error_count;
static uint32_t s_status_count;
static uint32_t s_message_count;
static uint32_t s_last_code;

static void test_log_error(uint32_t code)
{
    s_error_count++;
    s_last_code = code;
}

static void test_log_status(uint32_t code)
{
    s_status_count++;
    s_last_code = code;
}

static void test_log_message(uint32_t code)
{
    s_message_count++;
    s_last_code = code;
}

#define xLOG_ERROR(code, ...)   test_log_error((uint32_t)(code))
#define xLOG_STATUS(code)       test_log_status((uint32_t)(code))
#define xLOG_MESSAGE(code, ...) test_log_message((uint32_t)(code))

#include "xlog.h"

static void call_error_log(uint32_t code)
{
#if (xLOG_LEVEL_ERROR >= xLOG_LEVEL_MESSAGE)
    xLOG_MESSAGE(code, "error %u", 0U);
#elif (xLOG_LEVEL_ERROR >= xLOG_LEVEL_STATUS)
    do
    {
        xLOG_STATUS(code);
    } while (0);
#elif (xLOG_LEVEL_ERROR >= xLOG_LEVEL_ERROR)
    xLOG_ERROR(code, "error %u", 0U);
#else
    (void)(code);
#endif
}

static void call_status_log(uint32_t code)
{
#if (xLOG_LEVEL_STATUS >= xLOG_LEVEL_MESSAGE)
    xLOG_MESSAGE(code, "unused");
#elif (xLOG_LEVEL_STATUS >= xLOG_LEVEL_STATUS)
    do
    {
        xLOG_STATUS(code);
    } while (0);
#elif (xLOG_LEVEL_STATUS >= xLOG_LEVEL_ERROR)
    xLOG_ERROR(code, "unused");
#else
    (void)(code);
#endif
}

static void call_message_log(uint32_t code)
{
#if (xLOG_LEVEL_MESSAGE >= xLOG_LEVEL_MESSAGE)
    xLOG_MESSAGE(code, "message %u", 7U);
#elif (xLOG_LEVEL_MESSAGE >= xLOG_LEVEL_STATUS)
    do
    {
        xLOG_STATUS(code);
    } while (0);
#elif (xLOG_LEVEL_MESSAGE >= xLOG_LEVEL_ERROR)
    xLOG_ERROR(code, "message %u", 7U);
#else
    (void)(code);
#endif
}

// ---- xBYTES & xRETURN headers ----------------------------------------------

#include "xbytes.h"
#include "xreturn.h"

// ---- Test Phases -----------------------------------------------------------

static void test_phase_assert(void)
{
    uart_puts("--- Phase 1: xASSERT ---\n");

    s_assert_count = 0U;
    s_assert_file = NULL;
    s_assert_line = 0;
    s_assert_expr = NULL;
    s_assert_msg = NULL;

    // Test positive assert
    // cppcheck-suppress duplicateExpression
    xASSERT(1 == 1, "should pass");
    CHECK_EQUAL("true assert does not trigger handler", 0U, s_assert_count);

    // Test negative assert
    xASSERT(2 == 3, "should fail");
    CHECK_EQUAL("false assert triggers handler", 1U, s_assert_count);
    CHECK_TRUE("handler receives correct expression", s_assert_expr != NULL && s_assert_expr[0] == '2');
    CHECK_TRUE("handler receives correct message", s_assert_msg != NULL && s_assert_msg[0] == 's');
}

static void test_phase_bytes(void)
{
    uart_puts("--- Phase 2: xBYTES ---\n");

    // LE buffer read
    const uint8_t le_buf[] = {0x78U, 0x56U, 0x34U, 0x12U};
    CHECK_EQUAL("xRead_LE16", 0x5678U, xRead_LE16(&le_buf[0U]));
    CHECK_EQUAL("xRead_LE32", 0x12345678UL, xRead_LE32(&le_buf[0U]));

    // LE buffer write
    uint8_t write_buf[4U] = {0U, 0U, 0U, 0U};
    xWrite_LE16(write_buf, 0xABCDU);
    CHECK_EQUAL("xWrite_LE16 byte 0", 0xCDU, write_buf[0U]);
    CHECK_EQUAL("xWrite_LE16 byte 1", 0xABU, write_buf[1U]);

    xWrite_LE32(write_buf, 0x12345678UL);
    CHECK_EQUAL("xWrite_LE32 byte 0", 0x78U, write_buf[0U]);
    CHECK_EQUAL("xWrite_LE32 byte 3", 0x12U, write_buf[3U]);

    // BE buffer read
    const uint8_t be_buf[] = {0x12U, 0x34U, 0x56U, 0x78U};
    CHECK_EQUAL("xRead_BE16", 0x1234U, xRead_BE16(&be_buf[0U]));
    CHECK_EQUAL("xRead_BE32", 0x12345678UL, xRead_BE32(&be_buf[0U]));

    // Byte extractions
    CHECK_EQUAL("xU16_LOW_BYTE", 0xCDU, xU16_LOW_BYTE(0xABCDU));
    CHECK_EQUAL("xU16_HIGH_BYTE", 0xABU, xU16_HIGH_BYTE(0xABCDU));
    CHECK_EQUAL("xU32_BYTE0", 0x78U, xU32_BYTE0(0x12345678UL));
    CHECK_EQUAL("xU32_BYTE3", 0x12U, xU32_BYTE3(0x12345678UL));

    // Conversions and Swapping
    CHECK_EQUAL("xMAKE_U16", 0xABCDU, xMAKE_U16(0xCDU, 0xABU));
    CHECK_EQUAL("xMAKE_U32", 0x12345678UL, xMAKE_U32(0x78U, 0x56U, 0x34U, 0x12U));
    CHECK_EQUAL("xSWAP_U16", 0x3412U, xSWAP_U16(0x1234U));
    CHECK_EQUAL("xSWAP_U32", 0x78563412UL, xSWAP_U32(0x12345678UL));
}

static void test_phase_log(void)
{
    uart_puts("--- Phase 3: xLOG ---\n");

    s_error_count = 0U;
    s_status_count = 0U;
    s_message_count = 0U;
    s_last_code = 0U;

    // Error level
    call_error_log(0xDEADBEEFU);
    CHECK_EQUAL("error log routes to error backend", 1U, s_error_count);
    CHECK_EQUAL("error log does not route to status backend", 0U, s_status_count);
    CHECK_EQUAL("error log does not route to message backend", 0U, s_message_count);
    CHECK_EQUAL("error log captures code", 0xDEADBEEFU, s_last_code);

    // Status level
    call_status_log(0x12345678U);
    CHECK_EQUAL("status log routes to status backend", 1U, s_status_count);
    CHECK_EQUAL("status log does not route to message backend", 0U, s_message_count);
    CHECK_EQUAL("status log captures code", 0x12345678U, s_last_code);

    // Message level
    call_message_log(0x87654321U);
    CHECK_EQUAL("message log does not route to status backend", 1U, s_status_count);
    CHECK_EQUAL("message log routes to message backend", 1U, s_message_count);
    CHECK_EQUAL("message log captures code", 0x87654321U, s_last_code);
}

static void test_phase_return(void)
{
    uart_puts("--- Phase 4: xRETURN ---\n");

    // Packing and Unpacking
    xRETURN_t ret = xRETURN_MAKE(0x1234U, xRETURN_SEVERITY_ERROR, 0x0055U);
    CHECK_EQUAL("xRETURN_GET_MODULE", 0x1234U, xRETURN_GET_MODULE(ret));
    CHECK_EQUAL("xRETURN_GET_SEVERITY", xRETURN_SEVERITY_ERROR, xRETURN_GET_SEVERITY(ret));
    CHECK_EQUAL("xRETURN_GET_CODE", 0x0055U, xRETURN_GET_CODE(ret));

    // OK code checks
    CHECK_EQUAL("xRETURN_OK is 0", 0U, xRETURN_OK);
    CHECK_TRUE("xRETURN_IS_OK works on OK", xRETURN_IS_OK(xRETURN_OK));
    CHECK_TRUE("xRETURN_IS_ERROR false on OK", !xRETURN_IS_ERROR(xRETURN_OK));

    // Error and warning predicates
    xRETURN_t error = xRETURN_MAKE(0x0001U, xRETURN_SEVERITY_ERROR, 0x0001U);
    xRETURN_t warning = xRETURN_MAKE(0x0001U, xRETURN_SEVERITY_WARNING, 0x0002U);
    CHECK_TRUE("xRETURN_IS_ERROR matches error severity", xRETURN_IS_ERROR(error));
    CHECK_TRUE("xRETURN_IS_WARNING matches warning severity", xRETURN_IS_WARNING(warning));
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

    uart_puts("\n--- xUtil (xASSERT + xBYTES + xLOG + xRETURN) Cortex-R5 QEMU Smoke Test ---\n\n");

    s_pass = 0U;
    s_fail = 0U;

    test_phase_assert();
    test_phase_bytes();
    test_phase_log();
    test_phase_return();

    uart_puts("\n=== xUtil Results: PASS=");
    uart_puti(s_pass);
    uart_puts(" FAIL=");
    uart_puti(s_fail);
    uart_puts(" ===\n");

    if (s_fail == 0U)
    {
        uart_puts("ALL XUTIL TESTS PASSED SUCCESSFULLY!\n");
    }
    else
    {
        uart_puts("SOME XUTIL TESTS FAILED.\n");
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
