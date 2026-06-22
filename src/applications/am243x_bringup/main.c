#include <stddef.h>
#include <stdint.h>

#include "xrtos_core.h"
#include "xrtos_task.h"
#include "xrtos_tick.h"
#include "xrtos_port_arm_r5.h"
#include "xrtos_port_am243x.h"
#include "xsdk_soc_mmr.h"
#include "xuart.h"
#include "xuart_drv.h"
#include "xtimer.h"
#include "xtimer_drv.h"
#include "xi2c.h"
#include "xi2c_drv.h"
#include "xspi.h"
#include "xspi_drv.h"

#define UART0_BASE 0x02800000U
/* UART0 functional clock source:
 *   25 000 000 Hz - HFOSC0 oscillator (no SBL/TIFS: JTAG bare-metal load)
 *   48 000 000 Hz - MAIN_PLL0 HSDIV4_CLKOUT3 (when SBL/TIFS configures PLL) */
#define UART0_CLK_HZ    25000000U
#define UART0_BAUD      115200U
#define TIMER8_BASE     0x02480000U
#define TIMER8_IRQ      160U
#define TIMER8_CLK_HZ   25000000U
#define TICK_PERIOD_US  10000U /* 10 ms */
#define HEARTBEAT_TICKS 100U   /* 100 ticks = 1 s */

/* TIMER8 clock source mux - MAIN_CTRL_MMR0 partition 2 (0x43008000-0x4300BFFF) */
#define TIMER8_CLK_SRC_MUX_ADDR  (0x430081D0UL)
#define TIMER8_CLK_SRC_HFOSC0    (0x0UL) /* 25 MHz MCU_HFOSC0 */
#define TIMER8_CLK_MUX_PARTITION (2U)

#define I2C0_BASE_ADDR   0x02B10000U
#define I2C0_CLK_HZ      48000000U
#define MCSPI0_BASE_ADDR 0x02100000U
#define MCSPI0_CLK_HZ    50000000U

#define TASK_A_ID   0U
#define TASK_B_ID   1U
#define STACK_WORDS 128U

static uint32_t task_a_stack[STACK_WORDS];
static uint32_t task_b_stack[STACK_WORDS];
static uint32_t idle_stack[STACK_WORDS];

static xRTOS_Kernel_Context_t kernel_ctx;
static xRTOS_Task_Context_t task_a_ctx;
static xRTOS_Task_Context_t task_b_ctx;
static xRTOS_Task_Context_t idle_ctx;

static xUART_Context_t s_uart_ctx;
static xUART_AM243x_Context_t s_am243x_uart_ctx;

static xI2C_Context_t s_i2c_ctx;
static xI2C_AM243x_Context_t s_am243x_i2c_ctx;

static xSPI_Context_t s_spi_ctx;
static xSPI_AM243x_Context_t s_am243x_spi_ctx;

static xTIMER_Context_t s_timer_ctx;
static xTIMER_AM243x_Context_t s_am243x_timer_ctx;

static void uart_write(const char *s)
{
    if (s == NULL)
    {
        return;
    }
    size_t len = 0;
    while (s[len] != '\0')
    {
        len++;
    }
    if (len > 0)
    {
        (void)xUART_Transmit(&s_uart_ctx, (const uint8_t *)s, (uint32_t)len, 1000U);
    }
}

static void print_hex32(uint32_t val)
{
    const char hex_chars[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++)
    {
        buf[2 + i] = hex_chars[(val >> (28 - (i * 4))) & 0xFU];
    }
    buf[10] = '\0';
    uart_write(buf);
}

static void timer_isr(void *args)
{
    (void)args;
    xTIMER_Clear_IRQ(&s_timer_ctx);
    xRTOS_Port_AM243x_Tick_ISR(NULL);
}

static void task_a_entry(void *arg)
{
    (void)arg;
    for (;;)
    {
        uart_write("task A heartbeat\n");
        xRTOS_Task_Delay(HEARTBEAT_TICKS);
    }
}

static void task_b_entry(void *arg)
{
    (void)arg;
    for (;;)
    {
        uart_write("task B heartbeat\n");

        // ---- 1. HIL I2C Test ----
        {
            uint8_t dummy_rx_val = 0U;
            xRETURN_t i2c_ret = xI2C_Controller_Read(&s_i2c_ctx, 0x50, &dummy_rx_val, 1U, 100U);
            uart_write("[TEST] I2C read to 0x50: ret = ");
            print_hex32(i2c_ret);
            uart_write("\n");
        }

        // ---- 2. HIL SPI Test ----
        {
            uint8_t tx_buf[4] = {0x9F, 0x00, 0x00, 0x00};
            uint8_t rx_buf[4] = {0};
            xSPI_Device_t spi_dev = {.bus_ctx = &s_spi_ctx, .chip_select = 0U, .mode_flags = 0U};
            xSPI_Transaction_t spi_tx = {
                .clock_hz = 1000000U, .bits_per_word = 8U, .tx_buffer = tx_buf, .rx_buffer = rx_buf, .length = 4U, .timeout_ms = 100U};

            xRETURN_t spi_ret = xSPI_Transfer(&spi_dev, &spi_tx);
            uart_write("[TEST] SPI transfer 9F: ret = ");
            print_hex32(spi_ret);
            uart_write(" RX = ");
            for (uint32_t i = 0U; i < 4U; i++)
            {
                print_hex32(rx_buf[i]);
                uart_write(" ");
            }
            uart_write("\n");
        }

        xRTOS_Task_Delay(HEARTBEAT_TICKS);
    }
}

static void idle_entry(void *arg)
{
    (void)arg;
    for (;;)
        ;
}

int main(void)
{
    s_am243x_uart_ctx.base_addr = UART0_BASE;
    s_am243x_uart_ctx.input_clock_hz = UART0_CLK_HZ;

    xUART_Config_t uart_cfg = {.baud_rate = UART0_BAUD,
                               .data_bits = xUART_DATA_BITS_8,
                               .stop_bits = xUART_STOP_BITS_1,
                               .parity = xUART_PARITY_NONE,
                               .flow_control = xUART_FLOW_CONTROL_NONE};

    if (xUART_Init(&s_uart_ctx, &uart_cfg) != xRETURN_OK)
    {
        for (;;)
            ;
    }

    xUART_Start_Config_t uart_start_cfg = {.port = 0U, .drv_ops = &xUART_AM243x_Driver_Ops, .drv_ctx = &s_am243x_uart_ctx};

    if (xUART_Start(&s_uart_ctx, &uart_start_cfg) != xRETURN_OK)
    {
        for (;;)
            ;
    }

    uart_write("\nxSDK AM243x boot\n");

    // Initialize HIL I2C0
    {
        s_am243x_i2c_ctx.base_addr = I2C0_BASE_ADDR;
        s_am243x_i2c_ctx.input_clock_hz = I2C0_CLK_HZ;

        xI2C_Config_t i2c_cfg = {
            .bitrate_hz = 100000U, .address_mode = xI2C_ADDRESS_MODE_7_BIT, .has_own_address = false, .own_address = 0x00U};

        xI2C_Instance_t i2c_inst = {.ops = &xI2C_AM243x_Driver_Ops, .driver_ctx = &s_am243x_i2c_ctx};

        if (xI2C_Init(&s_i2c_ctx, &i2c_inst, &i2c_cfg) != xRETURN_OK)
        {
            uart_write("I2C Init failed\n");
        }
        else if (xI2C_Start(&s_i2c_ctx) != xRETURN_OK)
        {
            uart_write("I2C Start failed\n");
        }
        else
        {
            uart_write("I2C0 initialized at 100kHz\n");
        }
    }

    // Initialize HIL SPI0 (MCSPI0)
    {
        s_am243x_spi_ctx.base_addr = MCSPI0_BASE_ADDR;
        s_am243x_spi_ctx.input_clock_hz = MCSPI0_CLK_HZ;

        xSPI_Config_t spi_cfg = {
            .default_clock_hz = 1000000U, .default_mode_flags = 0U, .bits_per_word = 8U, .bit_order = xSPI_BIT_ORDER_MSB_FIRST};

        xSPI_Instance_t spi_inst = {.ops = &xSPI_AM243x_Driver_Ops, .driver_ctx = &s_am243x_spi_ctx};

        if (xSPI_Init(&s_spi_ctx, &spi_inst, &spi_cfg) != xRETURN_OK)
        {
            uart_write("SPI Init failed\n");
        }
        else if (xSPI_Start(&s_spi_ctx) != xRETURN_OK)
        {
            uart_write("SPI Start failed\n");
        }
        else
        {
            uart_write("SPI0 initialized at 1MHz\n");
        }
    }

    xRTOS_Port_AM243x_Init();

    /* Route TIMER8 clock to MCU_HFOSC0 (25 MHz). The mux register sits in
     * MAIN_CTRL_MMR0 partition 2 which requires KICK unlock before write. */
    xsdk_soc_mmr_unlock_main(TIMER8_CLK_MUX_PARTITION);
    *(volatile uint32_t *)TIMER8_CLK_SRC_MUX_ADDR = TIMER8_CLK_SRC_HFOSC0;
    xsdk_soc_mmr_lock_main(TIMER8_CLK_MUX_PARTITION);

    s_am243x_timer_ctx.base_addr = TIMER8_BASE;
    xTIMER_Config_t timer_cfg = {.period_us = TICK_PERIOD_US, .module_clk_hz = TIMER8_CLK_HZ};
    xTIMER_Instance_t timer_inst = {.ops = &xTIMER_AM243x_Driver_Ops, .driver_ctx = &s_am243x_timer_ctx};

    if (xTIMER_Init(&s_timer_ctx, &timer_inst, &timer_cfg) != xRETURN_OK)
    {
        for (;;)
            ;
    }

    xRTOS_Port_AM243x_Register_IRQ(TIMER8_IRQ, timer_isr, NULL, 15U, false);
    xRTOS_Port_AM243x_Enable_IRQ(TIMER8_IRQ);
    xTIMER_Start(&s_timer_ctx);

    xRTOS_Kernel_Init(&kernel_ctx, &xrtos_arm_r5_port_ops);

    xRTOS_Task_Config_t cfg;

    cfg = (xRTOS_Task_Config_t){.task_id = TASK_A_ID,
                                .priority = 1U,
                                .entry = task_a_entry,
                                .entry_arg = NULL,
                                .stack_mem = task_a_stack,
                                .stack_words = STACK_WORDS};
    xRTOS_Task_Create(&task_a_ctx, &cfg);

    cfg = (xRTOS_Task_Config_t){.task_id = TASK_B_ID,
                                .priority = 1U,
                                .entry = task_b_entry,
                                .entry_arg = NULL,
                                .stack_mem = task_b_stack,
                                .stack_words = STACK_WORDS};
    xRTOS_Task_Create(&task_b_ctx, &cfg);

    cfg = (xRTOS_Task_Config_t){.task_id = xRTOS_IDLE_TASK_ID,
                                .priority = xRTOS_IDLE_PRIORITY,
                                .entry = idle_entry,
                                .entry_arg = NULL,
                                .stack_mem = idle_stack,
                                .stack_words = STACK_WORDS};
    xRTOS_Task_Create(&idle_ctx, &cfg);

    uart_write("starting scheduler\n");

    xRTOS_Kernel_Start(); /* does not return */

    return 0;
}
