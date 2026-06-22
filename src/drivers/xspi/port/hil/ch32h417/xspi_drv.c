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

// @file xspi_drv.c
// @brief CH32H417 SPI hardware port implementation for the xSPI driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xspi_drv.h"
#include "xspi_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// Approximate CPU cycles consumed per polling iteration (1 MMIO read + compare + branch).
// Calibrates timeout_loops to wall-clock milliseconds without a hardware timer.
#define XSPI_CH32_CYCLES_PER_POLL_LOOP 8U

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static void      enable_clock(const xSPI_CH32H417_Context_t *ctx);
static void      reset_periph(const xSPI_CH32H417_Context_t *ctx);
static xRETURN_t ch32_init(void *driver_ctx, const xSPI_Config_t *config);
static xRETURN_t ch32_deinit(void *driver_ctx);
static xRETURN_t ch32_start(void *driver_ctx);
static xRETURN_t ch32_stop(void *driver_ctx);
static xRETURN_t ch32_set_event_callback(void *driver_ctx, xSPI_Driver_Event_Callback_t callback, void *callback_ctx);
static xRETURN_t ch32_transfer(void *driver_ctx, const xSPI_Device_t *device, const xSPI_Transaction_t *transaction);

const xSPI_Driver_Ops_t xSPI_CH32H417_Driver_Ops = {
    .init               = ch32_init,
    .deinit             = ch32_deinit,
    .start              = ch32_start,
    .stop               = ch32_stop,
    .set_event_callback = ch32_set_event_callback,
    .transfer           = ch32_transfer,
};

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static void enable_clock(const xSPI_CH32H417_Context_t *ctx)
{
    if (ctx->spi == SPI1)
    {
        RCC->HB2PCENR |= RCC_SPI1EN;
    }
    else if (ctx->spi == SPI2)
    {
        RCC->HB1PCENR |= RCC_SPI2EN;
    }
    else if (ctx->spi == SPI3)
    {
        RCC->HB1PCENR |= RCC_SPI3EN;
    }
    else if (ctx->spi == SPI4)
    {
        RCC->HB1PCENR |= RCC_SPI4EN;
    }
}

static void reset_periph(const xSPI_CH32H417_Context_t *ctx)
{
    if (ctx->spi == SPI1)
    {
        RCC->HB2PRSTR |= RCC_SPI1RST;
        RCC->HB2PRSTR &= ~RCC_SPI1RST;
    }
    else if (ctx->spi == SPI2)
    {
        RCC->HB1PRSTR |= RCC_SPI2RST;
        RCC->HB1PRSTR &= ~RCC_SPI2RST;
    }
    else if (ctx->spi == SPI3)
    {
        RCC->HB1PRSTR |= RCC_SPI3RST;
        RCC->HB1PRSTR &= ~RCC_SPI3RST;
    }
    else if (ctx->spi == SPI4)
    {
        RCC->HB1PRSTR |= RCC_SPI4RST;
        RCC->HB1PRSTR &= ~RCC_SPI4RST;
    }
}

static xRETURN_t ch32_init(void *driver_ctx, const xSPI_Config_t *config)
{
    xSPI_CH32H417_Context_t *ctx = (xSPI_CH32H417_Context_t *)driver_ctx;
    if ((ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (ctx->spi == NULL)
    {
        return xRETURN_xERR_xSPI_INVALID_ARG;
    }

    enable_clock(ctx);
    reset_periph(ctx);

    ctx->spi->CTLR1 = 0U;
    ctx->spi->CTLR2 = 0U;

    ctx->bit_order      = config->bit_order;
    ctx->is_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t ch32_deinit(void *driver_ctx)
{
    xSPI_CH32H417_Context_t *ctx = (xSPI_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    ctx->spi->CTLR1 = 0U;
    ctx->spi->CTLR2 = 0U;
    reset_periph(ctx);

    ctx->is_initialized = false;
    ctx->is_started     = false;
    ctx->is_busy        = false;

    return xRETURN_OK;
}

static xRETURN_t ch32_start(void *driver_ctx)
{
    xSPI_CH32H417_Context_t *ctx = (xSPI_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    // Configure basic SPI Master flags: MSTR, Software Slave Management (SSM=1, SSI=1), enable SPI
    ctx->spi->CTLR1 = (uint16_t)(SPI_CTLR1_MSTR | SPI_CTLR1_SSM | SPI_CTLR1_SSI | SPI_CTLR1_SPE);
    ctx->is_started = true;

    return xRETURN_OK;
}

static xRETURN_t ch32_stop(void *driver_ctx)
{
    xSPI_CH32H417_Context_t *ctx = (xSPI_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    ctx->spi->CTLR1 &= ~(uint16_t)SPI_CTLR1_SPE;
    ctx->is_started = false;

    return xRETURN_OK;
}

static xRETURN_t ch32_transfer(void *driver_ctx, const xSPI_Device_t *device, const xSPI_Transaction_t *transaction)
{
    xSPI_CH32H417_Context_t *ctx = (xSPI_CH32H417_Context_t *)driver_ctx;
    if ((ctx == NULL) || (device == NULL) || (transaction == NULL))
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (!ctx->is_started)
    {
        return xRETURN_xERR_xSPI_NOT_STARTED;
    }

    if (transaction->clock_hz == 0U)
    {
        return xRETURN_xERR_xSPI_INVALID_ARG;
    }

    ctx->is_busy = true;

    // 1. Calculate clock divisor BR[2:0]
    uint32_t ratio = ctx->pclk_hz / transaction->clock_hz;
    uint32_t br;
    if (ratio > 128U)
    {
        br = 7U; // div 256
    }
    else if (ratio > 64U)
    {
        br = 6U; // div 128
    }
    else if (ratio > 32U)
    {
        br = 5U; // div 64
    }
    else if (ratio > 16U)
    {
        br = 4U; // div 32
    }
    else if (ratio > 8U)
    {
        br = 3U; // div 16
    }
    else if (ratio > 4U)
    {
        br = 2U; // div 8
    }
    else if (ratio > 2U)
    {
        br = 1U; // div 4
    }
    else
    {
        br = 0U; // div 2
    }

    // 2. Build CTLR1 configuration
    uint16_t ctrl1 = (uint16_t)(SPI_CTLR1_MSTR | SPI_CTLR1_SSM | SPI_CTLR1_SSI | (br << 3U));

    // CPOL / CPHA
    if ((device->mode_flags & xSPI_MODE_CPOL_HIGH) != 0U)
    {
        ctrl1 |= SPI_CTLR1_CPOL;
    }
    if ((device->mode_flags & xSPI_MODE_CPHA_SECOND_EDGE) != 0U)
    {
        ctrl1 |= SPI_CTLR1_CPHA;
    }

    // Bit format (DFF 8/16 bit)
    if (transaction->bits_per_word == 16U)
    {
        ctrl1 |= SPI_CTLR1_DFF;
    }

    // LSBFIRST or MSBFIRST
    if (ctx->bit_order == xSPI_BIT_ORDER_LSB_FIRST)
    {
        ctrl1 |= SPI_CTLR1_LSBFIRST;
    }

    // Apply CTLR1 settings and enable SPI
    ctx->spi->CTLR1 = (uint16_t)(ctrl1 | SPI_CTLR1_SPE);

    // 3. Polling transfer loop
    uint32_t tx_idx = 0U;
    uint32_t rx_idx = 0U;
    uint32_t len    = transaction->length;

    // Calibrate iteration count to wall-clock: loops_per_ms ≈ pclk_hz / (cycles_per_loop * 1000).
    uint32_t loops_per_ms = ctx->pclk_hz / (XSPI_CH32_CYCLES_PER_POLL_LOOP * 1000U);
    if (loops_per_ms == 0U) { loops_per_ms = 1U; }
    uint32_t timeout_loops;
    if (transaction->timeout_ms == 0U)
    {
        timeout_loops = loops_per_ms * 1000U; // 1-second generous default
    }
    else
    {
        timeout_loops = (uint32_t)((uint64_t)transaction->timeout_ms * loops_per_ms);
    }

    uint32_t guard = 0U;
    if (transaction->bits_per_word == 16U)
    {
        const uint16_t *tx_buf = (const uint16_t *)transaction->tx_buffer;
        uint16_t       *rx_buf = (uint16_t *)transaction->rx_buffer;

        while (((tx_idx < len) || (rx_idx < len)) && (guard < timeout_loops))
        {
            uint16_t stat = ctx->spi->STATR;

            if ((tx_idx < len) && ((stat & SPI_STATR_TXE) != 0U))
            {
                uint16_t val = 0xFFFFU;
                if (tx_buf != NULL)
                {
                    val = tx_buf[tx_idx];
                }
                ctx->spi->DATAR = val;
                tx_idx++;
            }

            if ((rx_idx < len) && ((stat & SPI_STATR_RXNE) != 0U))
            {
                uint16_t val = ctx->spi->DATAR;
                if (rx_buf != NULL)
                {
                    rx_buf[rx_idx] = val;
                }
                rx_idx++;
            }

            guard++;
        }
    }
    else
    {
        const uint8_t *tx_buf = (const uint8_t *)transaction->tx_buffer;
        uint8_t       *rx_buf = (uint8_t *)transaction->rx_buffer;

        while (((tx_idx < len) || (rx_idx < len)) && (guard < timeout_loops))
        {
            uint16_t stat = ctx->spi->STATR;

            if ((tx_idx < len) && ((stat & SPI_STATR_TXE) != 0U))
            {
                uint16_t val = 0xFFU;
                if (tx_buf != NULL)
                {
                    val = tx_buf[tx_idx];
                }
                ctx->spi->DATAR = val;
                tx_idx++;
            }

            if ((rx_idx < len) && ((stat & SPI_STATR_RXNE) != 0U))
            {
                uint16_t val = ctx->spi->DATAR;
                if (rx_buf != NULL)
                {
                    rx_buf[rx_idx] = (uint8_t)(val & 0xFFU);
                }
                rx_idx++;
            }

            guard++;
        }
    }

    // Detect transfer timeout before proceeding to BSY drain.
    if ((tx_idx < len) || (rx_idx < len))
    {
        ctx->is_busy = false;
        return xRETURN_xERR_xSPI_TIMEOUT;
    }

    // Wait until SPI shift register drains (BSY clears).
    guard = 0U;
    while (((ctx->spi->STATR & SPI_STATR_BSY) != 0U) && (guard < timeout_loops))
    {
        guard++;
    }

    ctx->is_busy = false;

    if (guard >= timeout_loops)
    {
        return xRETURN_xERR_xSPI_TIMEOUT;
    }

    return xRETURN_OK;
}

static xRETURN_t ch32_set_event_callback(void *driver_ctx, xSPI_Driver_Event_Callback_t callback, void *callback_ctx)
{
    (void)driver_ctx;
    (void)callback;
    (void)callback_ctx;
    return xRETURN_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
