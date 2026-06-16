#include <stdint.h>

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
#define XSDK_CH32H417_RCC_HB2PCENR    (*(volatile uint32_t *)0x4002101CU)
#define XSDK_CH32H417_GPIOA_CFGHR     (*(volatile uint32_t *)0x40010804U)
#define XSDK_CH32H417_GPIOA_SPEED     (*(volatile uint32_t *)0x4001081CU)
#define XSDK_CH32H417_AFIO_GPIOA_AFHR (*(volatile uint32_t *)0x40010008U)
#define XSDK_CH32H417_USART1_STATR    (*(volatile uint16_t *)0x40013800U)
#define XSDK_CH32H417_USART1_DATAR    (*(volatile uint16_t *)0x40013804U)
#define XSDK_CH32H417_USART1_BRR      (*(volatile uint16_t *)0x40013808U)
#define XSDK_CH32H417_USART1_CTLR1    (*(volatile uint16_t *)0x4001380CU)

#define XSDK_CH32H417_RCC_AFIOEN   (1UL << 0)
#define XSDK_CH32H417_RCC_IOPAEN   (1UL << 2)
#define XSDK_CH32H417_RCC_USART1EN (1UL << 14)

#define XSDK_CH32H417_GPIO_PIN9_CFG_SHIFT   4U
#define XSDK_CH32H417_GPIO_PIN9_SPEED_SHIFT 18U
#define XSDK_CH32H417_GPIO_PIN9_AF_SHIFT    4U
#define XSDK_CH32H417_GPIO_FIELD_MASK       0xFU
#define XSDK_CH32H417_GPIO_SPEED_MASK       0x3U
#define XSDK_CH32H417_GPIO_OUTPUT_AF_PP     0x9U
#define XSDK_CH32H417_GPIO_SPEED_VERY_HIGH  0x3U
#define XSDK_CH32H417_GPIO_AF7              0x7U

#define XSDK_CH32H417_USART_TXE (1U << 7)
#define XSDK_CH32H417_USART_TE  (1U << 3)
#define XSDK_CH32H417_USART_UE  (1U << 13)

static xUART_Context_t s_uart_ctx;
static xUART_CH32H417_Context_t s_ch32_uart_ctx;

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
            GPIOC->BCR = (1UL << 3); // Turn Blue LED ON (active low)
        }
        else
        {
            GPIOC->BSHR = (1UL << 3); // Turn Blue LED OFF
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

    for (;;)
    {
        state = !state;
        if (state)
        {
            GPIOC->BCR = (1UL << 2); // Turn Green LED ON (active low)
        }
        else
        {
            GPIOC->BSHR = (1UL << 2); // Turn Green LED OFF
        }

        xSDK_SOC_UART_Write("task B heartbeat\r\n");
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

static uint32_t xSDK_CH32H417_GetHCLK(void)
{
    uint32_t sws = (*(volatile uint32_t *)0x40021004U) & 0x0000000CU;
    uint32_t sysclk = 25000000U;

    if (sws == 0x00U)
    {
        sysclk = 25000000U;
    }
    else if (sws == 0x04U)
    {
        sysclk = 25000000U;
    }
    else if (sws == 0x08U)
    {
        uint32_t syspll_sel = (*(volatile uint32_t *)0x40021008U) & 0x70000000U;
        if (syspll_sel == 0x00000000U)
        {
            uint32_t pllmull = (*(volatile uint32_t *)0x40021008U) & 0x0000001FU;
            uint32_t pllsource = (*(volatile uint32_t *)0x40021008U) & 0x000000E0U;
            uint32_t presc = (((*(volatile uint32_t *)0x40021008U) & 0x00003F00U) >> 8) + 1U;
            uint32_t tmp1 = 25000000U;

            if (pllsource == 0xA0U)
            {
                tmp1 = 500000000U / presc;
            }
            else if (pllsource == 0xE0U)
            {
                uint32_t serdes_mul_idx = ((*(volatile uint32_t *)0x40021034U) >> 16) & 0x0FU;
                const uint32_t serdes_mul_table[16] = {25, 28, 30, 32, 35, 38, 40, 45, 50, 56, 60, 64, 70, 76, 80, 90};
                uint32_t serdes_mul = serdes_mul_table[serdes_mul_idx];
                tmp1 = (25000000U * serdes_mul) / (2U * presc);
            }
            else if (pllsource == 0x80U)
            {
                tmp1 = 480000000U / presc;
            }
            else if (pllsource == 0xC0U)
            {
                tmp1 = 125000000U / presc;
            }
            else if (pllsource == 0x20U)
            {
                tmp1 = 25000000U / presc;
            }
            else
            {
                tmp1 = 25000000U / presc;
            }

            const uint32_t pll_mul_table[32] = {4,  6,  7,  8,  17, 9,  19, 10, 21, 11, 23, 12, 25, 13, 14, 15,
                                                16, 17, 18, 19, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 59};
            uint32_t pll_mul = pll_mul_table[pllmull];

            if ((pllmull == 4U) || (pllmull == 6U) || (pllmull == 8U) || (pllmull == 10U) || (pllmull == 12U))
            {
                sysclk = (tmp1 * pll_mul) >> 1U;
            }
            else
            {
                sysclk = tmp1 * pll_mul;
            }
        }
        else if (syspll_sel == 0x40000000U)
        {
            sysclk = 480000000U;
        }
        else if (syspll_sel == 0x50000000U)
        {
            sysclk = 500000000U;
        }
        else if (syspll_sel == 0x60000000U)
        {
            uint32_t serdes_mul_idx = ((*(volatile uint32_t *)0x40021034U) >> 16) & 0x0FU;
            const uint32_t serdes_mul_table[16] = {25, 28, 30, 32, 35, 38, 40, 45, 50, 56, 60, 64, 70, 76, 80, 90};
            uint32_t serdes_mul = serdes_mul_table[serdes_mul_idx];
            sysclk = (25000000U * serdes_mul) / 2U;
        }
        else if (syspll_sel == 0x70000000U)
        {
            sysclk = 125000000U;
        }
    }

    uint32_t hpre = ((*(volatile uint32_t *)0x40021004U) & 0x000000F0U) >> 4U;
    const uint32_t hb_presc_table[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};
    uint32_t sysclk_div = sysclk >> hb_presc_table[hpre];

    uint32_t fpre = ((*(volatile uint32_t *)0x40021004U) & 0x00030000U) >> 16U;
    const uint32_t fpre_table[4] = {0, 1, 2, 2};
    uint32_t hclk = sysclk_div >> fpre_table[fpre];

    return hclk;
}

int main(void)
{
    uint32_t observed_tick;
    uint32_t actual_hclk = xSDK_CH32H417_GetHCLK();

    // Pinmux and RCC clock configuration for USART1 (PA9/PA10)
    {
        uint32_t register_value;
        XSDK_CH32H417_RCC_HB2PCENR |= XSDK_CH32H417_RCC_AFIOEN | XSDK_CH32H417_RCC_IOPAEN | XSDK_CH32H417_RCC_USART1EN;

        register_value = XSDK_CH32H417_AFIO_GPIOA_AFHR;
        register_value &= ~(XSDK_CH32H417_GPIO_FIELD_MASK << XSDK_CH32H417_GPIO_PIN9_AF_SHIFT);
        register_value |= XSDK_CH32H417_GPIO_AF7 << XSDK_CH32H417_GPIO_PIN9_AF_SHIFT;
        XSDK_CH32H417_AFIO_GPIOA_AFHR = register_value;

        register_value = XSDK_CH32H417_GPIOA_CFGHR;
        register_value &= ~(XSDK_CH32H417_GPIO_FIELD_MASK << XSDK_CH32H417_GPIO_PIN9_CFG_SHIFT);
        register_value |= XSDK_CH32H417_GPIO_OUTPUT_AF_PP << XSDK_CH32H417_GPIO_PIN9_CFG_SHIFT;
        XSDK_CH32H417_GPIOA_CFGHR = register_value;

        register_value = XSDK_CH32H417_GPIOA_SPEED;
        register_value &= ~(XSDK_CH32H417_GPIO_SPEED_MASK << XSDK_CH32H417_GPIO_PIN9_SPEED_SHIFT);
        register_value |= XSDK_CH32H417_GPIO_SPEED_VERY_HIGH << XSDK_CH32H417_GPIO_PIN9_SPEED_SHIFT;
        XSDK_CH32H417_GPIOA_SPEED = register_value;
    }

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

    // Initialize LEDs on PC2 and PC3 (CH32H417 uses a new GPIO layout where Output PP is 0x1 in CFGLR and speed is set in SPEED register)
    *(volatile uint32_t *)0x4002101C |= (1UL << 4); // RCC_HB2Periph_GPIOC
    GPIOC->CFGLR &= ~0x0000FF00;                    // Clear configuration for pin 2 & 3
    GPIOC->CFGLR |= 0x00001100;                     // Set pin 2 & 3 to Output PP (0x1 per pin)
    GPIOC->SPEED &= ~0x000000F0;                    // Clear speed for pin 2 & 3
    GPIOC->SPEED |= 0x000000F0;                     // Set pin 2 & 3 to Very High Speed (0x3 per pin)
    GPIOC->BSHR = (1UL << 2) | (1UL << 3);          // Turn both LEDs OFF initially (high)

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
