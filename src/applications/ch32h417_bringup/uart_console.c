#include "uart_console.h"
#include <stddef.h>
#include "xuart.h"
#include "xuart_drv.h"
#include "ch32h417.h"

#define XSDK_CH32H417_UART_BAUD 115200U

static xUART_Context_t s_uart_ctx;
static xUART_CH32H417_Context_t s_ch32_uart_ctx;

void xSDK_Console_Init(uint32_t hclk_hz)
{
    s_ch32_uart_ctx.usart = USART1;
    s_ch32_uart_ctx.pclk_hz = hclk_hz;

    xUART_Config_t uart_cfg = {.baud_rate = XSDK_CH32H417_UART_BAUD,
                               .data_bits = xUART_DATA_BITS_8,
                               .stop_bits = xUART_STOP_BITS_1,
                               .parity = xUART_PARITY_NONE,
                               .flow_control = xUART_FLOW_CONTROL_NONE};

    if (xUART_Init(&s_uart_ctx, &uart_cfg) != xRETURN_OK)
    {
        for (;;)
        {
        }
    }

    xUART_Start_Config_t uart_start_cfg = {.port = 1U, .drv_ops = &xUART_CH32H417_Driver_Ops, .drv_ctx = &s_ch32_uart_ctx};

    if (xUART_Start(&s_uart_ctx, &uart_start_cfg) != xRETURN_OK)
    {
        for (;;)
        {
        }
    }
}

void xSDK_Console_PutChar(char character)
{
    (void)xUART_Transmit(&s_uart_ctx, (const uint8_t *)&character, 1U, 1000U);
}

void xSDK_Console_Write(const char *text)
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

void xSDK_Console_PrintHex32(uint32_t val)
{
    const char hex_chars[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4)
    {
        xSDK_Console_PutChar(hex_chars[(val >> i) & 0xF]);
    }
}
