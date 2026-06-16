#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_port_arm_r5.h"

#include "xusbd_core.h"
#include "xusbd_class.h"
#include "xusbd_return.h"
#include "xusbd_std.h"
#include "xusbd_dcd.h"

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

// ---- mock DCD implementation -----------------------------------------------

static xRETURN_t mock_dcd_init(void *dcd_ctx, USB_Speed_t speed, void *device_ctx)
{
    (void)dcd_ctx;
    (void)speed;
    (void)device_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_deinit(void *dcd_ctx)
{
    (void)dcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_set_callback(void *dcd_ctx, xUSBD_DCD_Event_Callback_t callback)
{
    (void)dcd_ctx;
    (void)callback;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_connect(void *dcd_ctx)
{
    (void)dcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_disconnect(void *dcd_ctx)
{
    (void)dcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_enable_interrupts(void *dcd_ctx)
{
    (void)dcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_disable_interrupts(void *dcd_ctx)
{
    (void)dcd_ctx;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_set_address(void *dcd_ctx, uint8_t address)
{
    (void)dcd_ctx;
    (void)address;
    return xRETURN_OK;
}

static USB_Speed_t mock_dcd_get_speed(void *dcd_ctx)
{
    (void)dcd_ctx;
    return USB_SPEED_HIGH;
}

static xRETURN_t mock_dcd_ep_init(void *dcd_ctx, uint8_t ep_addr, uint8_t ep_type, uint16_t mps)
{
    (void)dcd_ctx;
    (void)ep_addr;
    (void)ep_type;
    (void)mps;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_ep_deinit(void *dcd_ctx, uint8_t ep_addr)
{
    (void)dcd_ctx;
    (void)ep_addr;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_ep_receive(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)dcd_ctx;
    (void)ep_addr;
    (void)data;
    (void)length;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_ep_send(void *dcd_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length, bool is_zlp_required)
{
    (void)dcd_ctx;
    (void)ep_addr;
    (void)data;
    (void)length;
    (void)is_zlp_required;
    return xRETURN_OK;
}

static xRETURN_t mock_dcd_ep_stall(void *dcd_ctx, uint8_t ep_addr)
{
    (void)dcd_ctx;
    (void)ep_addr;
    return xRETURN_OK;
}

static xUSBD_DCD_Ops_t s_mock_dcd_ops = {
    .init = mock_dcd_init,
    .deinit = mock_dcd_deinit,
    .set_event_callback = mock_dcd_set_callback,
    .connect = mock_dcd_connect,
    .disconnect = mock_dcd_disconnect,
    .enable_interrupts = mock_dcd_enable_interrupts,
    .disable_interrupts = mock_dcd_disable_interrupts,
    .set_address = mock_dcd_set_address,
    .get_speed = mock_dcd_get_speed,
    .ep_init = mock_dcd_ep_init,
    .ep_deinit = mock_dcd_ep_deinit,
    .ep_receive = mock_dcd_ep_receive,
    .ep_send = mock_dcd_ep_send,
    .ep_stall = mock_dcd_ep_stall,
};

// ---- mock Class Driver -----------------------------------------------------

static xRETURN_t dummy_class_init(xUSBD_Class_Context_t *class_ctx)
{
    uint8_t interface = 0U;
    return xUSBD_Class_Allocate_Interface(class_ctx, &interface);
}

static uint32_t dummy_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    (void)class_ctx;
    (void)speed;
    buffer[0] = 9U;                         // length
    buffer[1] = 4U;                         // descriptor type (interface)
    buffer[2] = class_ctx->first_interface; // interface number
    buffer[3] = 0U;                         // alternate setting
    buffer[4] = 0U;                         // num endpoints
    buffer[5] = 0xFFU;                      // class (vendor)
    buffer[6] = 0x00U;                      // subclass
    buffer[7] = 0x00U;                      // protocol
    buffer[8] = 0U;                         // string index
    return 9U;
}

static xUSBD_Class_Driver_t s_dummy_class_driver = {
    .init_instance = dummy_class_init,
    .build_descriptor = dummy_build_descriptor,
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

    uart_puts("\n--- xUSBD Cortex-R5 QEMU Smoke Test ---\n\n");

    s_pass = 0U;
    s_fail = 0U;

    static xUSBD_Device_Context_t dev_ctx;
    static xUSBD_Class_Context_t class_ctx;

    xUSBD_Init_Config_t init_cfg = {
        .speed = USB_SPEED_HIGH,
        .vendor_string = (const uint8_t *)"XE",
        .product_string = (const uint8_t *)"xUSB",
        .serial_number_string = (const uint8_t *)"0001",
        .vendor_id = 0x1209U,
        .product_id = 0x0001U,
    };

    CHECK_OK("xUSBD_Init", xUSBD_Init(&dev_ctx, &init_cfg));
    CHECK_OK("xUSBD_Class_Register", xUSBD_Class_Register(&dev_ctx, &class_ctx, &s_dummy_class_driver));

    static int dummy_dcd_ctx = 0;
    xUSBD_Start_Config_t start_cfg = {
        .port = 0U,
        .dcd_ops = &s_mock_dcd_ops,
        .dcd_ctx = &dummy_dcd_ctx,
    };
    CHECK_OK("xUSBD_Start", xUSBD_Start(&dev_ctx, &start_cfg));

    bool started = false;
    CHECK_OK("xUSBD_Is_Started", xUSBD_Is_Started(&dev_ctx, &started));
    CHECK_TRUE("started == true", started);

    CHECK_OK("xUSBD_Stop", xUSBD_Stop(&dev_ctx));
    CHECK_OK("xUSBD_Is_Started (stopped)", xUSBD_Is_Started(&dev_ctx, &started));
    CHECK_TRUE("started == false", !started);

    uart_puts("\n=== xUSBD Results: PASS=");
    uart_puti(s_pass);
    uart_puts(" FAIL=");
    uart_puti(s_fail);
    uart_puts(" ===\n");

    if (s_fail == 0U)
    {
        uart_puts("ALL XUSBD TESTS PASSED SUCCESSFULLY!\n");
    }
    else
    {
        uart_puts("SOME XUSBD TESTS FAILED.\n");
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