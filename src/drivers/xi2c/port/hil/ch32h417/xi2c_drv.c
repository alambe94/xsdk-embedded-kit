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

// @file xi2c_drv.c
// @brief CH32H417 I2C hardware port implementation for the xI2C driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xi2c_drv.h"
#include "xi2c_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static void      enable_clock(const xI2C_CH32H417_Context_t *ctx);
static void      reset_periph(const xI2C_CH32H417_Context_t *ctx);
static xRETURN_t ch32_init(void *driver_ctx, const xI2C_Config_t *config);
static xRETURN_t ch32_deinit(void *driver_ctx);
static xRETURN_t ch32_start(void *driver_ctx);
static xRETURN_t ch32_stop(void *driver_ctx);
static xRETURN_t ch32_get_capabilities(const void *driver_ctx, xI2C_Capabilities_t *capabilities);
static xRETURN_t ch32_get_status(void *driver_ctx, xI2C_Status_t *status);
static xRETURN_t ch32_set_event_callback(void *driver_ctx, xI2C_Driver_Event_Callback_t callback, void *callback_ctx);
static xRETURN_t ch32_transfer(void *driver_ctx, const xI2C_Transaction_t *transaction);

const xI2C_Driver_Ops_t xI2C_CH32H417_Driver_Ops = {
    .init               = ch32_init,
    .deinit             = ch32_deinit,
    .start              = ch32_start,
    .stop               = ch32_stop,
    .get_capabilities   = ch32_get_capabilities,
    .get_status         = ch32_get_status,
    .set_event_callback = ch32_set_event_callback,
    .transfer           = ch32_transfer,
    .transfer_async     = NULL,
    .message_sequence   = NULL,
    .message_sequence_async = NULL,
    .acquire_bus        = NULL,
    .release_bus        = NULL,
    .abort              = NULL,
};

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static void enable_clock(const xI2C_CH32H417_Context_t *ctx)
{
    if (ctx->i2c == I2C1)
    {
        RCC->HB1PCENR |= RCC_I2C1EN;
    }
    else if (ctx->i2c == I2C2)
    {
        RCC->HB1PCENR |= RCC_I2C2EN;
    }
    else if (ctx->i2c == I2C3)
    {
        RCC->HB1PCENR |= RCC_I2C3EN;
    }
    else if (ctx->i2c == I2C4)
    {
        RCC->HB2PCENR |= RCC_I2C4EN;
    }
}

static void reset_periph(const xI2C_CH32H417_Context_t *ctx)
{
    if (ctx->i2c == I2C1)
    {
        RCC->HB1PRSTR |= RCC_I2C1RST;
        RCC->HB1PRSTR &= ~RCC_I2C1RST;
    }
    else if (ctx->i2c == I2C2)
    {
        RCC->HB1PRSTR |= RCC_I2C2RST;
        RCC->HB1PRSTR &= ~RCC_I2C2RST;
    }
    else if (ctx->i2c == I2C3)
    {
        RCC->HB1PRSTR |= RCC_I2C3RST;
        RCC->HB1PRSTR &= ~RCC_I2C3RST;
    }
    else if (ctx->i2c == I2C4)
    {
        RCC->HB2PRSTR |= RCC_I2C4RST;
        RCC->HB2PRSTR &= ~RCC_I2C4RST;
    }
}

static xRETURN_t ch32_init(void *driver_ctx, const xI2C_Config_t *config)
{
    xI2C_CH32H417_Context_t *ctx = (xI2C_CH32H417_Context_t *)driver_ctx;
    if ((ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (ctx->i2c == NULL)
    {
        return xRETURN_xERR_xI2C_INVALID_ARG;
    }

    enable_clock(ctx);
    reset_periph(ctx);

    // Disable peripheral first to configure registers
    ctx->i2c->CTLR1 = 0U;

    // Calculate input clock frequency in MHz
    uint32_t freq_mhz = ctx->pclk_hz / 1000000U;
    if (freq_mhz > 48U)
    {
        freq_mhz = 48U; // Cap at 48 MHz typical max for I2C input clock
    }
    if (freq_mhz < 2U)
    {
        freq_mhz = 2U; // Min is 2 MHz
    }

    // Set FREQ in CTLR2
    ctx->i2c->CTLR2 = (uint16_t)freq_mhz;

    // Configure CKCFGR and RTR based on target bitrate
    uint32_t ccr;
    uint32_t rtr;
    if (config->bitrate_hz <= 100000U)
    {
        // Standard Mode
        ccr = ctx->pclk_hz / (config->bitrate_hz * 2U);
        if (ccr < 4U)
        {
            ccr = 4U;
        }
        ctx->i2c->CKCFGR = (uint16_t)ccr;
        rtr = freq_mhz + 1U;
    }
    else
    {
        // Fast Mode (1:2 duty cycle)
        ccr = ctx->pclk_hz / (config->bitrate_hz * 3U);
        if (ccr < 1U)
        {
            ccr = 1U;
        }
        ctx->i2c->CKCFGR = (uint16_t)(ccr | 0x8000U); // Set F/S bit for Fast Mode
        rtr = ((freq_mhz * 300U) / 1000U) + 1U;
    }

    ctx->i2c->RTR = (uint16_t)rtr;

    // Configure own address if provided
    if (config->has_own_address)
    {
        ctx->i2c->OADDR1 = (uint16_t)(0x4000U | config->own_address);
    }

    ctx->is_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t ch32_deinit(void *driver_ctx)
{
    xI2C_CH32H417_Context_t *ctx = (xI2C_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    ctx->i2c->CTLR1 = 0U;
    reset_periph(ctx);

    ctx->is_initialized = false;
    ctx->is_started     = false;
    ctx->is_busy        = false;

    return xRETURN_OK;
}

static xRETURN_t ch32_start(void *driver_ctx)
{
    xI2C_CH32H417_Context_t *ctx = (xI2C_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    // Enable I2C peripheral and ACK
    ctx->i2c->CTLR1 |= (uint16_t)(I2C_CTLR1_PE | I2C_CTLR1_ACK);
    ctx->is_started = true;

    return xRETURN_OK;
}

static xRETURN_t ch32_stop(void *driver_ctx)
{
    xI2C_CH32H417_Context_t *ctx = (xI2C_CH32H417_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    ctx->i2c->CTLR1 &= ~(uint16_t)I2C_CTLR1_PE;
    ctx->is_started = false;

    return xRETURN_OK;
}

static xRETURN_t ch32_get_capabilities(const void *driver_ctx, xI2C_Capabilities_t *capabilities)
{
    (void)driver_ctx;
    if (capabilities == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    (void)memset(capabilities, 0, sizeof(*capabilities));
    capabilities->max_clock_hz = 400000U;

    return xRETURN_OK;
}

static xRETURN_t ch32_get_status(void *driver_ctx, xI2C_Status_t *status)
{
    xI2C_CH32H417_Context_t *ctx = (xI2C_CH32H417_Context_t *)driver_ctx;
    if ((ctx == NULL) || (status == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    status->is_initialized       = ctx->is_initialized;
    status->is_started           = ctx->is_started;
    status->is_busy              = ctx->is_busy;
    status->is_bus_acquired      = false;
    status->has_bus_error        = (ctx->i2c->STAR1 & I2C_STAR1_BERR) != 0U;
    status->has_arbitration_lost = (ctx->i2c->STAR1 & I2C_STAR1_ARLO) != 0U;
    status->last_error           = ctx->last_error;

    return xRETURN_OK;
}

static xRETURN_t ch32_set_event_callback(void *driver_ctx, xI2C_Driver_Event_Callback_t callback, void *callback_ctx)
{
    (void)driver_ctx;
    (void)callback;
    (void)callback_ctx;
    // Async callbacks are not supported for this polling implementation
    return xRETURN_xERR_xI2C_UNSUPPORTED;
}

static xRETURN_t ch32_transfer(void *driver_ctx, const xI2C_Transaction_t *transaction)
{
    xI2C_CH32H417_Context_t *ctx = (xI2C_CH32H417_Context_t *)driver_ctx;
    if ((ctx == NULL) || (transaction == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    uint32_t timeout_loops = transaction->timeout_ms * 1000U;
    if (timeout_loops == 0U)
    {
        timeout_loops = 1000000U;
    }

    xRETURN_t status = xRETURN_OK;
    I2C_TypeDef *i2c = ctx->i2c;

    bool has_write = (transaction->tx_buffer != NULL) && (transaction->tx_length > 0U);
    bool has_read  = (transaction->rx_buffer != NULL) && (transaction->rx_length > 0U);

    if (!has_write && !has_read)
    {
        return xRETURN_xERR_xI2C_INVALID_ARG;
    }

    ctx->is_busy = true;

    // Wait until bus is not busy
    uint32_t guard = 0U;
    while ((i2c->STAR2 & I2C_STAR2_BUSY) != 0U)
    {
        if (++guard >= timeout_loops)
        {
            ctx->is_busy = false;
            return xRETURN_xERR_xI2C_TIMEOUT;
        }
    }

    // --- WRITE PHASE ---
    if (has_write)
    {
        i2c->CTLR1 |= I2C_CTLR1_ACK;
        i2c->CTLR1 |= I2C_CTLR1_START;

        // Wait for SB (Start Bit)
        guard = 0U;
        while ((i2c->STAR1 & I2C_STAR1_SB) == 0U)
        {
            if ((i2c->STAR1 & I2C_STAR1_AF) != 0U)
            {
                i2c->STAR1 &= ~I2C_STAR1_AF;
                i2c->CTLR1 |= I2C_CTLR1_STOP;
                ctx->is_busy = false;
                return xRETURN_xERR_xI2C_NACK;
            }
            if (++guard >= timeout_loops)
            {
                i2c->CTLR1 |= I2C_CTLR1_STOP;
                ctx->is_busy = false;
                return xRETURN_xERR_xI2C_TIMEOUT;
            }
        }

        // Send write address
        i2c->DATAR = (uint16_t)((transaction->device_address << 1U) & 0xFEU);

        // Wait for ADDR
        guard = 0U;
        while ((i2c->STAR1 & I2C_STAR1_ADDR) == 0U)
        {
            if ((i2c->STAR1 & I2C_STAR1_AF) != 0U)
            {
                i2c->STAR1 &= ~I2C_STAR1_AF;
                i2c->CTLR1 |= I2C_CTLR1_STOP;
                ctx->is_busy = false;
                return xRETURN_xERR_xI2C_NACK;
            }
            if (++guard >= timeout_loops)
            {
                i2c->CTLR1 |= I2C_CTLR1_STOP;
                ctx->is_busy = false;
                return xRETURN_xERR_xI2C_TIMEOUT;
            }
        }

        // Clear ADDR flag
        (void)i2c->STAR1;
        (void)i2c->STAR2;

        // Write data payload
        for (uint32_t i = 0U; i < transaction->tx_length; i++)
        {
            // Wait for TXE
            guard = 0U;
            while ((i2c->STAR1 & I2C_STAR1_TXE) == 0U)
            {
                if ((i2c->STAR1 & I2C_STAR1_AF) != 0U)
                {
                    i2c->STAR1 &= ~I2C_STAR1_AF;
                    i2c->CTLR1 |= I2C_CTLR1_STOP;
                    ctx->is_busy = false;
                    return xRETURN_xERR_xI2C_NACK;
                }
                if (++guard >= timeout_loops)
                {
                    i2c->CTLR1 |= I2C_CTLR1_STOP;
                    ctx->is_busy = false;
                    return xRETURN_xERR_xI2C_TIMEOUT;
                }
            }

            i2c->DATAR = (uint16_t)transaction->tx_buffer[i];
        }

        // Wait for BTF
        guard = 0U;
        while ((i2c->STAR1 & I2C_STAR1_BTF) == 0U)
        {
            if ((i2c->STAR1 & I2C_STAR1_AF) != 0U)
            {
                i2c->STAR1 &= ~I2C_STAR1_AF;
                i2c->CTLR1 |= I2C_CTLR1_STOP;
                ctx->is_busy = false;
                return xRETURN_xERR_xI2C_NACK;
            }
            if (++guard >= timeout_loops)
            {
                i2c->CTLR1 |= I2C_CTLR1_STOP;
                ctx->is_busy = false;
                return xRETURN_xERR_xI2C_TIMEOUT;
            }
        }

        if (!has_read)
        {
            i2c->CTLR1 |= I2C_CTLR1_STOP;
        }
    }

    // --- READ PHASE ---
    if (has_read)
    {
        i2c->CTLR1 |= I2C_CTLR1_ACK;
        i2c->CTLR1 |= I2C_CTLR1_START;

        // Wait for SB (Repeated Start or initial Start)
        guard = 0U;
        while ((i2c->STAR1 & I2C_STAR1_SB) == 0U)
        {
            if (++guard >= timeout_loops)
            {
                i2c->CTLR1 |= I2C_CTLR1_STOP;
                ctx->is_busy = false;
                return xRETURN_xERR_xI2C_TIMEOUT;
            }
        }

        // Send read address
        i2c->DATAR = (uint16_t)((transaction->device_address << 1U) | 1U);

        // Wait for ADDR
        guard = 0U;
        while ((i2c->STAR1 & I2C_STAR1_ADDR) == 0U)
        {
            if ((i2c->STAR1 & I2C_STAR1_AF) != 0U)
            {
                i2c->STAR1 &= ~I2C_STAR1_AF;
                i2c->CTLR1 |= I2C_CTLR1_STOP;
                ctx->is_busy = false;
                return xRETURN_xERR_xI2C_NACK;
            }
            if (++guard >= timeout_loops)
            {
                i2c->CTLR1 |= I2C_CTLR1_STOP;
                ctx->is_busy = false;
                return xRETURN_xERR_xI2C_TIMEOUT;
            }
        }

        // Clear ADDR flag
        (void)i2c->STAR1;
        (void)i2c->STAR2;

        uint32_t bytes_to_read = transaction->rx_length;
        uint8_t *rx_ptr        = transaction->rx_buffer;

        if (bytes_to_read == 1U)
        {
            i2c->CTLR1 &= ~(uint16_t)I2C_CTLR1_ACK;
            i2c->CTLR1 |= I2C_CTLR1_STOP;

            guard = 0U;
            while ((i2c->STAR1 & I2C_STAR1_RXNE) == 0U)
            {
                if (++guard >= timeout_loops)
                {
                    ctx->is_busy = false;
                    return xRETURN_xERR_xI2C_TIMEOUT;
                }
            }

            *rx_ptr = (uint8_t)(i2c->DATAR & 0xFFU);
        }
        else
        {
            for (uint32_t i = 0U; i < bytes_to_read; i++)
            {
                if (i == (bytes_to_read - 1U))
                {
                    i2c->CTLR1 &= ~(uint16_t)I2C_CTLR1_ACK;
                    i2c->CTLR1 |= I2C_CTLR1_STOP;
                }

                guard = 0U;
                while ((i2c->STAR1 & I2C_STAR1_RXNE) == 0U)
                {
                    if (++guard >= timeout_loops)
                    {
                        ctx->is_busy = false;
                        return xRETURN_xERR_xI2C_TIMEOUT;
                    }
                }

                rx_ptr[i] = (uint8_t)(i2c->DATAR & 0xFFU);
            }
        }

        i2c->CTLR1 |= I2C_CTLR1_ACK;
    }

    ctx->is_busy = false;

    return status;
}

// EOF /////////////////////////////////////////////////////////////////////////////
