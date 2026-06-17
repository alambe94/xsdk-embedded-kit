#include <stdint.h>
#include <stdbool.h>

#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"
#include "xrtos_core.h"
#include "xrtos_defs.h"
#include "xrtos_port_ch32h417.h"
#include "xrtos_return.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xuart.h"
#include "xuart_drv.h"
#include "xsdk_port_ch32h417.h"
#include "xi2c.h"
#include "xi2c_drv.h"
#include "xspi.h"
#include "xspi_drv.h"
#include "xtimer.h"

static xUART_Context_t s_uart_ctx;
static xUART_CH32H417_Context_t s_ch32_uart_ctx;

static xI2C_Context_t s_i2c_ctx;
static xI2C_CH32H417_Context_t s_ch32_i2c_ctx;

static xSPI_Context_t s_spi_ctx;
static xSPI_CH32H417_Context_t s_ch32_spi_ctx;

static void xSDK_SOC_UART_PutChar(char character)
{
    (void)xUART_Transmit(&s_uart_ctx, (const uint8_t *)&character, 1U, 1000U);
}

static void xSDK_SOC_UART_Write(const char *text)
{
    if (text == NULL)
    {
        return;
    }
    size_t len = 0;
    while (text[len] != '\0')
    {
        len++;
    }
    if (len > 0)
    {
        (void)xUART_Transmit(&s_uart_ctx, (const uint8_t *)text, (uint32_t)len, 1000U);
    }
}
#include "xusbd_core.h"
#include "xusbd_drv.h"
#include "xusbd_win.h"

#define XSDK_CH32H417_BOOT_HCLK_HZ 100000000U
#define XSDK_CH32H417_UART_BAUD    115200U
#define XSDK_CH32H417_TICK_HZ      100U

#ifndef XSDK_CH32H417_ENABLE_XRTOS_SCHEDULER
#define XSDK_CH32H417_ENABLE_XRTOS_SCHEDULER 1
#endif

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
static xRTOS_Task_Context_t xSDK_CH32H417_Task_B;
static uint32_t xSDK_CH32H417_Idle_Stack[XSDK_CH32H417_TASK_STACK_WORDS];
static uint32_t xSDK_CH32H417_Task_A_Stack[XSDK_CH32H417_TASK_STACK_WORDS];
static uint32_t xSDK_CH32H417_Task_B_Stack[XSDK_CH32H417_TASK_STACK_WORDS];
static xUSBD_Device_Context_t xSDK_CH32H417_USB_Device;
static xUSBD_WIN_Context_t xSDK_CH32H417_USB_WIN;

static void xSDK_CH32H417_Boot_Fail(const char *message);
static void xSDK_CH32H417_Idle_Entry(void *arg);
static void xSDK_CH32H417_Task_A_Entry(void *arg);
static void xSDK_CH32H417_Task_B_Entry(void *arg);
static void xSDK_CH32H417_Create_Task(xRTOS_Task_Context_t *task_ctx,
                                      uint32_t task_id,
                                      uint32_t priority,
                                      xRTOS_Task_Entry_t entry,
                                      uint32_t *stack_mem,
                                      const char *name);
static void xSDK_CH32H417_Init_xRTOS(void);

static void xSDK_CH32H417_Boot_Fail(const char *message)
{
    xSDK_SOC_UART_Write(message);
    for (;;)
    {
    }
}

static void xSDK_CH32H417_Idle_Entry(void *arg)
{
    (void)arg;

    for (;;)
    {
        __asm volatile("wfi");
    }
}

static void xSDK_CH32H417_PrintHex32(uint32_t val)
{
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4)
    {
        xSDK_SOC_UART_PutChar(hex_chars[(val >> i) & 0xF]);
    }
}

static void xSDK_CH32H417_Task_A_Entry(void *arg)
{
    (void)arg;
    static bool state = false;

    for (;;)
    {
        state = !state;
        if (state)
        {
            xGPIO_Pin_Write(GPIOC, 3U, false); // Turn Blue LED ON (active low)
        }
        else
        {
            xGPIO_Pin_Write(GPIOC, 3U, true); // Turn Blue LED OFF
        }

        xSDK_SOC_UART_Write("Task A: CTLR=0x");
        xSDK_CH32H417_PrintHex32(*(volatile uint32_t *)0x40021000);
        xSDK_SOC_UART_Write(" CFGR0=0x");
        xSDK_CH32H417_PrintHex32(*(volatile uint32_t *)0x40021004);
        xSDK_SOC_UART_Write(" LINK_STATUS=0x");
        xSDK_CH32H417_PrintHex32(*(volatile uint32_t *)0x40034010);
        xSDK_SOC_UART_Write("\r\n");
        (void)xRTOS_Task_Delay(XSDK_CH32H417_TASK_A_DELAY_TICKS);
    }
}

static void xSDK_CH32H417_Task_B_Entry(void *arg)
{
    (void)arg;
    static bool state = false;
    static uint32_t tim2_ticks = 0U;

    for (;;)
    {
        state = !state;
        if (state)
        {
            xGPIO_Pin_Write(GPIOC, 2U, false); // Turn Green LED ON (active low)
        }
        else
        {
            xGPIO_Pin_Write(GPIOC, 2U, true); // Turn Green LED OFF
        }

        // ---- 1. Timer Test ----
        if ((TIM2->INTFR & TIM_UIF) != 0U)
        {
            xTIMER_Clear_IRQ(TIM2_BASE);
            tim2_ticks++;
            xSDK_SOC_UART_Write("[TEST] TIM2 tick count = 0x");
            xSDK_CH32H417_PrintHex32(tim2_ticks);
            xSDK_SOC_UART_Write(" (TIM2->CNT = 0x");
            xSDK_CH32H417_PrintHex32(TIM2->CNT);
            xSDK_SOC_UART_Write(")\r\n");
        }

        // ---- 2. I2C Test ----
        {
            uint8_t dummy_rx_val = 0U;
            xRETURN_t i2c_ret = xI2C_Controller_Read(&s_i2c_ctx, 0x50, &dummy_rx_val, 1U, 100U);
            xSDK_SOC_UART_Write("[TEST] I2C Read to 0x50: ret = 0x");
            xSDK_CH32H417_PrintHex32(i2c_ret);
            xSDK_SOC_UART_Write(" (expected NACK: 0x000F0008)\r\n");
        }

        // ---- 3. SPI Test ----
        {
            uint8_t tx_buf[4] = {0x9F, 0x00, 0x00, 0x00};
            uint8_t rx_buf[4] = {0};
            xSPI_Device_t spi_dev = {.bus_ctx = &s_spi_ctx, .chip_select = 0U, .mode_flags = 0U};
            xSPI_Transaction_t spi_tx = {
                .clock_hz = 1000000U, .bits_per_word = 8U, .tx_buffer = tx_buf, .rx_buffer = rx_buf, .length = 4U, .timeout_ms = 100U};

            xGPIO_Pin_Write(GPIOA, 4U, false); // Assert CS
            xRETURN_t spi_ret = xSPI_Transfer(&spi_dev, &spi_tx);
            xGPIO_Pin_Write(GPIOA, 4U, true); // Deassert CS

            xSDK_SOC_UART_Write("[TEST] SPI Transfer 9F: ret = 0x");
            xSDK_CH32H417_PrintHex32(spi_ret);
            xSDK_SOC_UART_Write(" RX = ");
            for (uint32_t i = 0U; i < 4U; i++)
            {
                xSDK_CH32H417_PrintHex32(rx_buf[i]);
                xSDK_SOC_UART_Write(" ");
            }
            xSDK_SOC_UART_Write("\r\n");
        }

        (void)xRTOS_Task_Delay(XSDK_CH32H417_TASK_B_DELAY_TICKS);
    }
}

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
        xSDK_CH32H417_Boot_Fail("xRTOS task create failed\r\n");
    }
}

static void xSDK_CH32H417_Init_xRTOS(void)
{
    if (xRTOS_Kernel_Init(&xSDK_CH32H417_Kernel, &xrtos_ch32h417_port_ops) != xRETURN_xRTOS_OK)
    {
        xSDK_CH32H417_Boot_Fail("xRTOS init failed\r\n");
    }

    xSDK_CH32H417_Create_Task(&xSDK_CH32H417_Idle_Task, xRTOS_IDLE_TASK_ID, xRTOS_IDLE_PRIORITY, xSDK_CH32H417_Idle_Entry,
                              xSDK_CH32H417_Idle_Stack, "idle");
    xSDK_CH32H417_Create_Task(&xSDK_CH32H417_Task_A, XSDK_CH32H417_TASK_A_ID, XSDK_CH32H417_TASK_A_PRIORITY, xSDK_CH32H417_Task_A_Entry,
                              xSDK_CH32H417_Task_A_Stack, "task_a");
    xSDK_CH32H417_Create_Task(&xSDK_CH32H417_Task_B, XSDK_CH32H417_TASK_B_ID, XSDK_CH32H417_TASK_B_PRIORITY, xSDK_CH32H417_Task_B_Entry,
                              xSDK_CH32H417_Task_B_Stack, "task_b");
}

int main(void)
{
    uint32_t observed_tick;
    uint32_t actual_hclk = xRCC_Get_HCLK_Freq();

    xSDK_Port_Init();
    xSDK_Port_USART1_Pinmux_Init();

    // Set up and start proper xUART driver
    s_ch32_uart_ctx.usart = USART1;
    s_ch32_uart_ctx.pclk_hz = actual_hclk;

    xUART_Config_t uart_cfg = {.baud_rate = XSDK_CH32H417_UART_BAUD,
                               .data_bits = xUART_DATA_BITS_8,
                               .stop_bits = xUART_STOP_BITS_1,
                               .parity = xUART_PARITY_NONE,
                               .flow_control = xUART_FLOW_CONTROL_NONE,
                               .callbacks.on_event = NULL};

    if (xUART_Init(&s_uart_ctx, &uart_cfg) != xRETURN_OK)
    {
        xSDK_CH32H417_Boot_Fail("xUART Init failed\r\n");
    }

    xUART_Start_Config_t uart_start_cfg = {.port = 1U, .drv_ops = &xUART_CH32H417_Driver_Ops, .drv_ctx = &s_ch32_uart_ctx};

    if (xUART_Start(&s_uart_ctx, &uart_start_cfg) != xRETURN_OK)
    {
        xSDK_CH32H417_Boot_Fail("xUART Start failed\r\n");
    }

    xSDK_SOC_UART_Write("xSDK CH32H417 boot\r\n");

    // Initialize LEDs on PC2 and PC3
    xRCC_Enable_Periph_Clock(xRCC_PERIPH_GPIOC);

    xGPIO_Config_t led_cfg = {.mode = xGPIO_MODE_OUTPUT_PP, .speed = xGPIO_SPEED_VERY_HIGH};
    xGPIO_Init(GPIOC, 2U, &led_cfg);
    xGPIO_Init(GPIOC, 3U, &led_cfg);

    xGPIO_Pin_Write(GPIOC, 2U, true); // Turn both LEDs OFF initially (high)
    xGPIO_Pin_Write(GPIOC, 3U, true);

    // -------------------------------------------------------------------------
    // Test Peripherals Configuration (I2C1, SPI1, TIM2)
    // -------------------------------------------------------------------------

    // 1. Configure I2C1 pinmux (PB6=SCL, PB7=SDA)
    {
        xSDK_Port_I2C1_Pinmux_Init();

        s_ch32_i2c_ctx.i2c = I2C1;
        s_ch32_i2c_ctx.pclk_hz = actual_hclk;

        xI2C_Config_t i2c_cfg = {
            .bitrate_hz = 100000U, .address_mode = xI2C_ADDRESS_MODE_7_BIT, .has_own_address = false, .own_address = 0x00U};

        xI2C_Instance_t i2c_inst = {.ops = &xI2C_CH32H417_Driver_Ops, .driver_ctx = &s_ch32_i2c_ctx};

        if (xI2C_Init(&s_i2c_ctx, &i2c_inst, &i2c_cfg) != xRETURN_OK)
        {
            xSDK_SOC_UART_Write("I2C Init failed\r\n");
        }
        else if (xI2C_Start(&s_i2c_ctx) != xRETURN_OK)
        {
            xSDK_SOC_UART_Write("I2C Start failed\r\n");
        }
        else
        {
            xSDK_SOC_UART_Write("I2C1 initialized at 100kHz\r\n");
        }
    }

    // 2. Configure SPI1 pinmux (PA5=SCK, PA6=MISO, PA7=MOSI, PA4=CS)
    {
        xSDK_Port_SPI1_Pinmux_Init();

        s_ch32_spi_ctx.spi = SPI1;
        s_ch32_spi_ctx.pclk_hz = actual_hclk;

        xSPI_Config_t spi_cfg = {
            .default_clock_hz = 1000000U, .default_mode_flags = 0U, .bits_per_word = 8U, .bit_order = xSPI_BIT_ORDER_MSB_FIRST};

        xSPI_Instance_t spi_inst = {.ops = &xSPI_CH32H417_Driver_Ops, .driver_ctx = &s_ch32_spi_ctx};

        if (xSPI_Init(&s_spi_ctx, &spi_inst, &spi_cfg) != xRETURN_OK)
        {
            xSDK_SOC_UART_Write("SPI Init failed\r\n");
        }
        else if (xSPI_Start(&s_spi_ctx) != xRETURN_OK)
        {
            xSDK_SOC_UART_Write("SPI Start failed\r\n");
        }
        else
        {
            xSDK_SOC_UART_Write("SPI1 initialized at 1MHz\r\n");
        }
    }

    // 3. Configure TIM2 Timer for 1-second interval
    {
        xTIMER_Init_Periodic(TIM2_BASE, 1000000U, actual_hclk);
        xTIMER_Start(TIM2_BASE);
        xSDK_SOC_UART_Write("TIM2 started (1s period)\r\n");
    }

    xSDK_CH32H417_Init_xRTOS();
    xSDK_SOC_UART_Write("xRTOS tasks registered\r\n");

    xRTOS_Port_CH32H417_Timer_Init(actual_hclk, XSDK_CH32H417_TICK_HZ);

    // Initialize USB device stack
    xUSBD_Init_Config_t usb_init_config = {
        .speed = USB_SPEED_SUPER,
        .vendor_string = (const uint8_t *)"alambe94",
        .product_string = (const uint8_t *)"xSDK CH32H417 USBSS WinUSB",
        .serial_number_string = (const uint8_t *)"123456",
        .vendor_id = 0x1209,
        .product_id = 0x0001,
    };
    if (xUSBD_Init(&xSDK_CH32H417_USB_Device, &usb_init_config) == xRETURN_OK)
    {
        xSDK_SOC_UART_Write("USB stack initialized\r\n");
        if (xUSBD_Class_Register(&xSDK_CH32H417_USB_Device, &xSDK_CH32H417_USB_WIN.class_ctx, xUSBD_WIN_Class()) == xRETURN_OK)
        {
            xUSBD_Start_Config_t usb_start_config = {
                .port = 0,
                .dcd_ops = &xUSBD_CH32H417_DCD_Ops,
                .dcd_ctx = &xUSBD_CH32H417_DCD_Context,
            };
            if (xUSBD_Start(&xSDK_CH32H417_USB_Device, &usb_start_config) == xRETURN_OK)
            {
                xSDK_SOC_UART_Write("USBSS DCD started\r\n");
            }
            else
            {
                xSDK_SOC_UART_Write("Failed to start USBSS DCD\r\n");
            }
        }
        else
        {
            xSDK_SOC_UART_Write("Failed to register WinUSB class\r\n");
        }
    }
    else
    {
        xSDK_SOC_UART_Write("Failed to initialize USB stack\r\n");
    }

#if XSDK_CH32H417_ENABLE_XRTOS_SCHEDULER
    if (xRTOS_Kernel_Start() != xRETURN_xRTOS_OK)
    {
        xSDK_CH32H417_Boot_Fail("xRTOS scheduler start failed\r\n");
    }
#endif

    observed_tick = 0U;
    for (;;)
    {
        uint32_t current_tick = xRTOS_Tick_Get();

        if ((current_tick - observed_tick) >= XSDK_CH32H417_TICK_HZ)
        {
            observed_tick += XSDK_CH32H417_TICK_HZ;
            xSDK_CH32H417_Heartbeat_Tick = observed_tick;
            xSDK_SOC_UART_PutChar('.');
        }
    }
}
