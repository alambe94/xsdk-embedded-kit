#include "test_peripherals.h"
#include <stddef.h>
#include "xi2c.h"
#include "xi2c_drv.h"
#include "xspi.h"
#include "xspi_drv.h"
#include "xtimer.h"
#include "xtimer_drv.h"
#include "xgpio.h"
#include "xsdk_port_ch32h417.h"
#include "ch32h417.h"
#include "uart_console.h"

static xI2C_Context_t s_i2c_ctx;
static xI2C_CH32H417_Context_t s_ch32_i2c_ctx;

static xSPI_Context_t s_spi_ctx;
static xSPI_CH32H417_Context_t s_ch32_spi_ctx;

static xTIMER_Context_t s_timer_ctx;
static xTIMER_CH32H417_Context_t s_ch32_timer_ctx;

static uint32_t s_tim2_ticks = 0U;

void xSDK_I2C_Test_Init(uint32_t hclk_hz)
{
    xSDK_Port_I2C1_Pinmux_Init();

    s_ch32_i2c_ctx.i2c = I2C1;
    s_ch32_i2c_ctx.pclk_hz = hclk_hz;

    xI2C_Config_t i2c_cfg = {
        .bitrate_hz = 100000U, .address_mode = xI2C_ADDRESS_MODE_7_BIT, .has_own_address = false, .own_address = 0x00U};

    xI2C_Instance_t i2c_inst = {.ops = &xI2C_CH32H417_Driver_Ops, .driver_ctx = &s_ch32_i2c_ctx};

    if (xI2C_Init(&s_i2c_ctx, &i2c_inst, &i2c_cfg) != xRETURN_OK)
    {
        xSDK_Console_Write("I2C Init failed\r\n");
    }
    else if (xI2C_Start(&s_i2c_ctx) != xRETURN_OK)
    {
        xSDK_Console_Write("I2C Start failed\r\n");
    }
    else
    {
        xSDK_Console_Write("I2C1 initialized at 100kHz\r\n");
    }
}

void xSDK_SPI_Test_Init(uint32_t hclk_hz)
{
    xSDK_Port_SPI1_Pinmux_Init();

    s_ch32_spi_ctx.spi = SPI1;
    s_ch32_spi_ctx.pclk_hz = hclk_hz;

    xSPI_Config_t spi_cfg = {
        .default_clock_hz = 1000000U, .default_mode_flags = 0U, .bits_per_word = 8U, .bit_order = xSPI_BIT_ORDER_MSB_FIRST};

    xSPI_Instance_t spi_inst = {.ops = &xSPI_CH32H417_Driver_Ops, .driver_ctx = &s_ch32_spi_ctx};

    if (xSPI_Init(&s_spi_ctx, &spi_inst, &spi_cfg) != xRETURN_OK)
    {
        xSDK_Console_Write("SPI Init failed\r\n");
    }
    else if (xSPI_Start(&s_spi_ctx) != xRETURN_OK)
    {
        xSDK_Console_Write("SPI Start failed\r\n");
    }
    else
    {
        xSDK_Console_Write("SPI1 initialized at 1MHz\r\n");
    }
}

void xSDK_Timer_Test_Init(uint32_t hclk_hz)
{
    s_ch32_timer_ctx.base_addr = TIM2_BASE;
    xTIMER_Config_t timer_cfg = {.period_us = 1000000U, .module_clk_hz = hclk_hz};
    xTIMER_Instance_t timer_inst = {.ops = &xTIMER_CH32H417_Driver_Ops, .driver_ctx = &s_ch32_timer_ctx};

    if (xTIMER_Init(&s_timer_ctx, &timer_inst, &timer_cfg) != xRETURN_OK)
    {
        xSDK_Console_Write("Timer Init failed\r\n");
    }
    else if (xTIMER_Start(&s_timer_ctx) != xRETURN_OK)
    {
        xSDK_Console_Write("Timer Start failed\r\n");
    }
    else
    {
        xSDK_Console_Write("TIM2 started (1s period)\r\n");
    }
}

void xSDK_Timer_Test_Run(void)
{
    if ((TIM2->INTFR & TIM_UIF) != 0U)
    {
        (void)xTIMER_Clear_IRQ(&s_timer_ctx);
        s_tim2_ticks++;
        xSDK_Console_Write("[TEST] TIM2 tick count = 0x");
        xSDK_Console_PrintHex32(s_tim2_ticks);
        xSDK_Console_Write(" (TIM2->CNT = 0x");
        xSDK_Console_PrintHex32(TIM2->CNT);
        xSDK_Console_Write(")\r\n");
    }
}

void xSDK_I2C_Test_Run(void)
{
    uint8_t dummy_rx_val = 0U;
    xRETURN_t i2c_ret = xI2C_Controller_Read(&s_i2c_ctx, 0x50, &dummy_rx_val, 1U, 100U);
    xSDK_Console_Write("[TEST] I2C Read to 0x50: ret = 0x");
    xSDK_Console_PrintHex32(i2c_ret);
    xSDK_Console_Write(" (expected NACK: 0x000F0008)\r\n");
}

void xSDK_SPI_Test_Run(void)
{
    uint8_t tx_buf[4] = {0x9F, 0x00, 0x00, 0x00};
    uint8_t rx_buf[4] = {0};
    xSPI_Device_t spi_dev = {.bus_ctx = &s_spi_ctx, .chip_select = 0U, .mode_flags = 0U};
    xSPI_Transaction_t spi_tx = {
        .clock_hz = 1000000U, .bits_per_word = 8U, .tx_buffer = tx_buf, .rx_buffer = rx_buf, .length = 4U, .timeout_ms = 100U};

    (void)xGPIO_Pin_Write(&g_gpio_a_ctx, 4U, false); // Assert CS
    xRETURN_t spi_ret = xSPI_Transfer(&spi_dev, &spi_tx);
    (void)xGPIO_Pin_Write(&g_gpio_a_ctx, 4U, true); // Deassert CS

    xSDK_Console_Write("[TEST] SPI Transfer 9F: ret = 0x");
    xSDK_Console_PrintHex32(spi_ret);
    xSDK_Console_Write(" RX = ");
    for (uint32_t i = 0U; i < 4U; i++)
    {
        xSDK_Console_PrintHex32(rx_buf[i]);
        xSDK_Console_Write(" ");
    }
    xSDK_Console_Write("\r\n");
}
