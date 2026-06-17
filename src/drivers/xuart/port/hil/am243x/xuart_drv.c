// Copyright 2026 alambe94
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xuart_drv.c
// @brief TI AM243x UART hardware port implementation for the xUART driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xuart_drv.h"
#include "xuart_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////
/* AM243x UART register offsets (word-addressed: each 8-bit reg occupies one 32-bit word) */
#define UART_THR    0x00U   /* Transmit Holding Register (DLAB=0, write) */
#define UART_DLL    0x00U   /* Divisor Latch Low         (DLAB=1) */
#define UART_IER    0x04U   /* Interrupt Enable Register (DLAB=0) */
#define UART_DLH    0x04U   /* Divisor Latch High        (DLAB=1) */
#define UART_FCR    0x08U   /* FIFO Control Register     (write) */
#define UART_LCR    0x0CU   /* Line Control Register */
#define UART_LSR    0x14U   /* Line Status Register */
#define UART_MDR1   0x20U   /* TI Mode Definition Register 1 (not in standard 16550) */

#define UART_LCR_DLAB       (1U << 7)
#define UART_LCR_8N1        0x03U
#define UART_FCR_FIFO_EN    0x07U       /* enable + reset TX and RX FIFOs */
#define UART_LSR_THRE       (1U << 5)
#define UART_LSR_TEMT       (1U << 6)
#define UART_MDR1_DISABLE   0x07U       /* MODESELECT=111: IP disabled (reset default) */
#define UART_MDR1_UART16X   0x00U       /* MODESELECT=000: 16x UART (normal polling) */

/* AM243x MAIN_PADCFG_CTRL_MMR - pin mux configuration for UART0 TX/RX.
 * PADCFG base: 0x000F0000, PMUX region starts at +0x4000.
 * UART0_RXD/TXD offsets: from TI csl_pinmux am64x_am243x PIN_UART0_RXD/TXD.
 * KICK unlock required before writing pad config registers (reset = locked).
 * Two partitions: LOCK0 KICK0/1 at +0x1008/0x100C, LOCK1 KICK0/1 at +0x5008/0x500C. */
#define PADCFG_BASE             0x000F0000U
#define PADCFG_PMUX_OFF         0x4000U
#define PADCFG_LOCK0_KICK0_OFF  0x1008U
#define PADCFG_LOCK0_KICK1_OFF  0x100CU
#define PADCFG_LOCK1_KICK0_OFF  0x5008U
#define PADCFG_LOCK1_KICK1_OFF  0x500CU
#define PADCFG_KICK0_UNLOCK     0x68EF3490U
#define PADCFG_KICK1_UNLOCK     0xD172BC5AU
#define PADCFG_KICK_LOCK        0x00000000U

#define UART0_RXD_PAD_OFF   0x0230U
#define UART0_TXD_PAD_OFF   0x0234U

/* Pad config value fields (from TI pinmux.h for am64x_am243x):
 *   bit 16: PULLUDEN  - 1 = disable pull resistor
 *   bit 17: PULLTYPESEL - 1 = pull-up (when pull enabled)
 *   bit 18: INPUT_EN  - 1 = enable input receiver */
#define PAD_MODE(m)         ((uint32_t)(m))
#define PAD_PULL_DISABLE    (1U << 16U)
#define PAD_PULL_UP         (1U << 17U)
#define PAD_INPUT_EN        (1U << 18U)

/* TX: mode 0, pull disabled, output */
#define UART0_TXD_PAD_VAL   (PAD_MODE(0U) | PAD_PULL_DISABLE)
/* RX: mode 0, pull-up enabled, input receiver on */
#define UART0_RXD_PAD_VAL   (PAD_MODE(0U) | PAD_PULL_UP | PAD_INPUT_EN)

#define REG32(base, off) (*(volatile uint32_t *)((base) + (off)))

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xUART_AM243x_Context_t *as_port_context(void *driver_ctx);

static xRETURN_t am243x_init(void *driver_ctx, const xUART_Config_t *config);
static xRETURN_t am243x_deinit(void *driver_ctx);
static xRETURN_t am243x_start(void *driver_ctx);
static xRETURN_t am243x_stop(void *driver_ctx);
static xRETURN_t am243x_set_event_callback(void *driver_ctx, xUART_Driver_Event_Callback_t callback, void *callback_ctx);
static xRETURN_t am243x_transmit(void *driver_ctx, const uint8_t *buffer, uint32_t length, uint32_t timeout_ms);
static xRETURN_t am243x_receive(void *driver_ctx, uint8_t *buffer, uint32_t length, uint32_t timeout_ms);

const xUART_Driver_Ops_t xUART_AM243x_Driver_Ops = {
    .init               = am243x_init,
    .deinit             = am243x_deinit,
    .start              = am243x_start,
    .stop               = am243x_stop,
    .set_event_callback = am243x_set_event_callback,
    .transmit           = am243x_transmit,
    .receive            = am243x_receive,
    .transmit_async     = NULL,
    .receive_async      = NULL,
    .abort_tx           = NULL,
    .abort_rx           = NULL,
};

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xUART_AM243x_Context_t *as_port_context(void *driver_ctx)
{
    return (xUART_AM243x_Context_t *)driver_ctx;
}

static xRETURN_t am243x_init(void *driver_ctx, const xUART_Config_t *config)
{
    xUART_AM243x_Context_t *ctx;
    uint32_t                lcr = 0U;

    if ((driver_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    if (ctx->base_addr == 0U)
    {
        return xRETURN_xERR_xUART_INVALID_ARG;
    }

    if (config->baud_rate == 0U || ctx->input_clock_hz == 0U)
    {
        /* Skip baud rate/pinmux setup, just enable IP */
        REG32(ctx->base_addr, UART_MDR1) = UART_MDR1_UART16X;
        ctx->is_initialized = true;
        return xRETURN_OK;
    }

    /* Pinmux logic for UART0 */
    if (ctx->base_addr == 0x02800000U)
    {
        REG32(PADCFG_BASE, PADCFG_LOCK0_KICK0_OFF) = PADCFG_KICK0_UNLOCK;
        REG32(PADCFG_BASE, PADCFG_LOCK0_KICK1_OFF) = PADCFG_KICK1_UNLOCK;
        REG32(PADCFG_BASE, PADCFG_LOCK1_KICK0_OFF) = PADCFG_KICK0_UNLOCK;
        REG32(PADCFG_BASE, PADCFG_LOCK1_KICK1_OFF) = PADCFG_KICK1_UNLOCK;

        uint32_t padbase = PADCFG_BASE + PADCFG_PMUX_OFF;
        REG32(padbase, UART0_TXD_PAD_OFF) = UART0_TXD_PAD_VAL;
        REG32(padbase, UART0_RXD_PAD_OFF) = UART0_RXD_PAD_VAL;

        REG32(PADCFG_BASE, PADCFG_LOCK0_KICK0_OFF) = PADCFG_KICK_LOCK;
        REG32(PADCFG_BASE, PADCFG_LOCK1_KICK0_OFF) = PADCFG_KICK_LOCK;
    }

    /* Round to nearest divisor: (clk + 8*baud) / (16*baud) */
    uint32_t divisor = (ctx->input_clock_hz + (8U * config->baud_rate)) / (16U * config->baud_rate);
    if (divisor == 0U)
    {
        divisor = 1U;
    }

    /* LCR setting based on config options */
    switch (config->data_bits)
    {
        case xUART_DATA_BITS_5: lcr |= 0x00U; break;
        case xUART_DATA_BITS_6: lcr |= 0x01U; break;
        case xUART_DATA_BITS_7: lcr |= 0x02U; break;
        case xUART_DATA_BITS_8: lcr |= 0x03U; break;
        default:                return xRETURN_xERR_xUART_INVALID_ARG;
    }

    switch (config->stop_bits)
    {
        case xUART_STOP_BITS_1:   lcr |= 0x00U; break;
        case xUART_STOP_BITS_2:   lcr |= 0x04U; break;
        default:                  return xRETURN_xERR_xUART_INVALID_ARG;
    }

    switch (config->parity)
    {
        case xUART_PARITY_NONE: /* parity disabled */ break;
        case xUART_PARITY_EVEN: lcr |= 0x18U; break; /* PEN = 1, EPS = 1 */
        case xUART_PARITY_ODD:  lcr |= 0x08U; break; /* PEN = 1, EPS = 0 */
        default:                return xRETURN_xERR_xUART_INVALID_ARG;
    }

    REG32(ctx->base_addr, UART_MDR1) = UART_MDR1_DISABLE; /* hold in reset while reconfiguring */

    REG32(ctx->base_addr, UART_LCR) = lcr | UART_LCR_DLAB; /* unlock divisor latches */
    REG32(ctx->base_addr, UART_DLL) = divisor & 0xFFU;
    REG32(ctx->base_addr, UART_DLH) = (divisor >> 8U) & 0xFFU;

    REG32(ctx->base_addr, UART_LCR) = lcr; /* DLAB=0, set data width, parity, stop bits */
    REG32(ctx->base_addr, UART_FCR) = UART_FCR_FIFO_EN;
    REG32(ctx->base_addr, UART_IER) = 0x00U; /* polling mode: no interrupts */

    REG32(ctx->base_addr, UART_MDR1) = UART_MDR1_UART16X; /* enable */

    ctx->is_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t am243x_deinit(void *driver_ctx)
{
    xUART_AM243x_Context_t *ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    REG32(ctx->base_addr, UART_MDR1) = UART_MDR1_DISABLE; /* disable IP */

    ctx->is_initialized = false;
    ctx->is_started     = false;
    ctx->is_tx_busy     = false;
    ctx->is_rx_busy     = false;

    return xRETURN_OK;
}

static xRETURN_t am243x_start(void *driver_ctx)
{
    xUART_AM243x_Context_t *ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    REG32(ctx->base_addr, UART_IER) = 0x00U; /* polling mode: no interrupts */
    REG32(ctx->base_addr, UART_MDR1) = UART_MDR1_UART16X; /* enable */

    ctx->is_started = true;

    return xRETURN_OK;
}

static xRETURN_t am243x_stop(void *driver_ctx)
{
    xUART_AM243x_Context_t *ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    REG32(ctx->base_addr, UART_MDR1) = UART_MDR1_DISABLE; /* disable */

    ctx->is_started = false;

    return xRETURN_OK;
}

static xRETURN_t am243x_set_event_callback(void *driver_ctx, xUART_Driver_Event_Callback_t callback, void *callback_ctx)
{
    xUART_AM243x_Context_t *ctx;

    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx = as_port_context(driver_ctx);

    ctx->event_callback     = callback;
    ctx->event_callback_ctx = callback_ctx;

    return xRETURN_OK;
}

static xRETURN_t am243x_transmit(void *driver_ctx, const uint8_t *buffer, uint32_t length, uint32_t timeout_ms)
{
    xUART_AM243x_Context_t *ctx;
    uint32_t                i;
    uint32_t                deadline;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx      = as_port_context(driver_ctx);
    deadline = timeout_ms * 1000U;

    for (i = 0U; i < length; i++)
    {
        uint32_t guard = 0U;
        while (!(REG32(ctx->base_addr, UART_LSR) & UART_LSR_THRE))
        {
            if (++guard >= deadline)
            {
                return xRETURN_xERR_xUART_TIMEOUT;
            }
        }
        REG32(ctx->base_addr, UART_THR) = (uint32_t)buffer[i];
    }

    /* Wait for transmitter to be completely empty */
    {
        uint32_t guard = 0U;
        while (!(REG32(ctx->base_addr, UART_LSR) & UART_LSR_TEMT))
        {
            if (++guard >= deadline)
            {
                return xRETURN_xERR_xUART_TIMEOUT;
            }
        }
    }

    return xRETURN_OK;
}

static xRETURN_t am243x_receive(void *driver_ctx, uint8_t *buffer, uint32_t length, uint32_t timeout_ms)
{
    xUART_AM243x_Context_t *ctx;
    uint32_t                i;
    uint32_t                deadline;

    if ((driver_ctx == NULL) || (buffer == NULL))
    {
        return xRETURN_xERR_xUART_NULL_POINTER;
    }

    ctx      = as_port_context(driver_ctx);
    deadline = timeout_ms * 1000U;

    for (i = 0U; i < length; i++)
    {
        uint32_t guard = 0U;
        /* Wait for Data Ready (LSR bit 0) */
        while (!(REG32(ctx->base_addr, UART_LSR) & 0x01U))
        {
            if (++guard >= deadline)
            {
                return xRETURN_xERR_xUART_TIMEOUT;
            }
        }
        buffer[i] = (uint8_t)(REG32(ctx->base_addr, UART_THR) & 0xFFU);
    }

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// EOF /////////////////////////////////////////////////////////////////////////////
