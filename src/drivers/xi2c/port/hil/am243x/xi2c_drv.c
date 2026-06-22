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
// @brief TI AM243x hardware port implementation for the xI2C driver core.
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

// CSL INCLUDES
#include <drivers/i2c/v0/cslr_i2c.h>

// MACROS //////////////////////////////////////////////////////////////////////////
#define I2C_MAX_CLK_PRESCALAR       (255U)
#define I2C_INTERNAL_CLK_STEP       (1000000U)

#define I2C_MODULE_INTERNAL_CLK_4MHZ  (4000000U)
#define I2C_MODULE_INTERNAL_CLK_12MHZ (12000000U)

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t reset_controller(CSL_I2cRegs *regs);
static void      set_clk_config(CSL_I2cRegs *regs, uint32_t sys_clk, uint32_t bitrate_hz);
static xRETURN_t am243x_init(void *driver_ctx, const xI2C_Config_t *config);
static xRETURN_t am243x_deinit(void *driver_ctx);
static xRETURN_t am243x_start(void *driver_ctx);
static xRETURN_t am243x_stop(void *driver_ctx);
static xRETURN_t am243x_set_event_callback(void *driver_ctx, xI2C_Driver_Event_Callback_t callback, void *callback_ctx);
static xRETURN_t am243x_transfer(void *driver_ctx, const xI2C_Transaction_t *transaction);

const xI2C_Driver_Ops_t xI2C_AM243x_Driver_Ops = {
    .init                   = am243x_init,
    .deinit                 = am243x_deinit,
    .start                  = am243x_start,
    .stop                   = am243x_stop,
    .set_event_callback     = am243x_set_event_callback,
    .transfer               = am243x_transfer,
    .transfer_async         = NULL,
    .message_sequence       = NULL,
    .message_sequence_async = NULL,
    .acquire_bus            = NULL,
    .release_bus            = NULL,
    .abort                  = NULL,
};

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t reset_controller(CSL_I2cRegs *regs)
{
    uint32_t timeout = 100000U;

    regs->CON &= ~I2C_CON_I2C_EN_MASK;
    regs->SYSC |= I2C_SYSC_SRST_MASK;
    regs->CON |= I2C_CON_I2C_EN_MASK;

    while (((regs->SYSS & I2C_SYSS_RDONE_MASK) == 0U) && (timeout > 0U))
    {
        timeout--;
    }

    if (timeout == 0U)
    {
        return xRETURN_xERR_xI2C_TIMEOUT;
    }

    regs->CON &= ~I2C_CON_I2C_EN_MASK;
    return xRETURN_OK;
}

static void set_clk_config(CSL_I2cRegs *regs, uint32_t sys_clk, uint32_t bitrate_hz)
{
    uint32_t prescalar;
    uint32_t internal_clk;
    uint32_t act_int_clk = 0U;

    if (bitrate_hz <= 100000U)
    {
        internal_clk = I2C_MODULE_INTERNAL_CLK_4MHZ;
    }
    else
    {
        internal_clk = I2C_MODULE_INTERNAL_CLK_12MHZ;
    }

    for (prescalar = 0U; prescalar < I2C_MAX_CLK_PRESCALAR; prescalar++)
    {
        act_int_clk = sys_clk / (prescalar + 1U);
        if (act_int_clk <= (internal_clk + I2C_INTERNAL_CLK_STEP))
        {
            break;
        }
    }

    if (bitrate_hz > 400000U)
    {
        prescalar   = 0U;
        act_int_clk = sys_clk;
    }
    regs->PSC = prescalar;

    uint32_t divisor = act_int_clk / bitrate_hz;
    if ((bitrate_hz * divisor) != act_int_clk)
    {
        divisor += 1U;
    }

    uint32_t div_h = divisor / 2U;
    uint32_t div_l = divisor - div_h;

    if (bitrate_hz > 400000U)
    {
        regs->SCLL = (uint32_t)((div_l - 7U) << 8U);
        regs->SCLH = (uint32_t)((div_h - 5U) << 8U);
    }
    else
    {
        regs->SCLL = (uint32_t)(div_l - 7U);
        regs->SCLH = (uint32_t)(div_h - 5U);
    }
}

static xRETURN_t am243x_init(void *driver_ctx, const xI2C_Config_t *config)
{
    xI2C_AM243x_Context_t *ctx = (xI2C_AM243x_Context_t *)driver_ctx;
    if ((ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (ctx->is_initialized == true)
    {
        return xRETURN_xERR_xI2C_INVALID_STATE;
    }

    if (ctx->base_addr == 0U)
    {
        return xRETURN_xERR_xI2C_INVALID_ARG;
    }

    CSL_I2cRegs *regs = (CSL_I2cRegs *)ctx->base_addr;

    xRETURN_t status = reset_controller(regs);
    if (status != xRETURN_OK)
    {
        return status;
    }

    set_clk_config(regs, ctx->input_clock_hz, config->bitrate_hz);

    uint32_t sysc = 0U;
    sysc |= (CSL_I2C_SYSC_AUTOIDLE_DISABLE << CSL_I2C_SYSC_AUTOIDLE_SHIFT) & CSL_I2C_SYSC_AUTOIDLE_MASK;
    sysc |= (CSL_I2C_SYSC_IDLEMODE_NOIDLE << CSL_I2C_SYSC_IDLEMODE_SHIFT) & CSL_I2C_SYSC_IDLEMODE_MASK;
    sysc |= (CSL_I2C_SYSC_ENAWAKEUP_DISABLE << CSL_I2C_SYSC_ENAWAKEUP_SHIFT) & CSL_I2C_SYSC_ENAWAKEUP_MASK;
    sysc |= ((uint32_t)CSL_I2C_SYSC_CLKACTIVITY_BOOTHOFF << CSL_I2C_SYSC_CLKACTIVITY_SHIFT) & CSL_I2C_SYSC_CLKACTIVITY_MASK;
    regs->SYSC = sysc;

    uint32_t con = 0U;
    con |= (I2C_CON_OPMODE_FSI2C << I2C_CON_OPMODE_SHIFT) & I2C_CON_OPMODE_MASK;
    con |= (I2C_CON_STB_NORMAL << I2C_CON_STB_SHIFT) & I2C_CON_STB_MASK;
    regs->CON = con;

    if (config->has_own_address)
    {
        regs->OA = config->own_address;
    }

    regs->CON |= I2C_CON_I2C_EN_MASK;
    regs->SYSTEST |= I2C_SYSTEST_FREE_MASK;

    regs->IRQSTATUS = 0x7FFFU;

    ctx->is_initialized = true;
    ctx->is_started     = false;
    ctx->is_busy        = false;

    return xRETURN_OK;
}

static xRETURN_t am243x_deinit(void *driver_ctx)
{
    xI2C_AM243x_Context_t *ctx = (xI2C_AM243x_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (ctx->is_initialized == false)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    CSL_I2cRegs *regs = (CSL_I2cRegs *)ctx->base_addr;
    regs->CON &= ~I2C_CON_I2C_EN_MASK;
    regs->SYSC |= I2C_SYSC_SRST_MASK;

    ctx->is_initialized = false;
    ctx->is_started     = false;
    ctx->is_busy        = false;

    return xRETURN_OK;
}

static xRETURN_t am243x_start(void *driver_ctx)
{
    xI2C_AM243x_Context_t *ctx = (xI2C_AM243x_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (ctx->is_initialized == false)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    CSL_I2cRegs *regs = (CSL_I2cRegs *)ctx->base_addr;
    regs->CON |= I2C_CON_I2C_EN_MASK;

    ctx->is_started = true;

    return xRETURN_OK;
}

static xRETURN_t am243x_stop(void *driver_ctx)
{
    xI2C_AM243x_Context_t *ctx = (xI2C_AM243x_Context_t *)driver_ctx;
    if (ctx == NULL)
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    CSL_I2cRegs *regs = (CSL_I2cRegs *)ctx->base_addr;
    regs->CON &= ~I2C_CON_I2C_EN_MASK;

    ctx->is_started = false;

    return xRETURN_OK;
}


static xRETURN_t am243x_set_event_callback(void *driver_ctx, xI2C_Driver_Event_Callback_t callback, void *callback_ctx)
{
    (void)driver_ctx;
    (void)callback;
    (void)callback_ctx;
    return xRETURN_xERR_xI2C_UNSUPPORTED;
}

static xRETURN_t am243x_transfer(void *driver_ctx, const xI2C_Transaction_t *transaction)
{
    xI2C_AM243x_Context_t *ctx = (xI2C_AM243x_Context_t *)driver_ctx;
    if ((ctx == NULL) || (transaction == NULL))
    {
        return xRETURN_xERR_xI2C_NULL_POINTER;
    }

    if (ctx->is_initialized == false)
    {
        return xRETURN_xERR_xI2C_NOT_INITIALIZED;
    }

    if (ctx->is_started == false)
    {
        return xRETURN_xERR_xI2C_NOT_STARTED;
    }

    CSL_I2cRegs *regs = (CSL_I2cRegs *)ctx->base_addr;

    bool has_write = (transaction->tx_buffer != NULL) && (transaction->tx_length > 0U);
    bool has_read  = (transaction->rx_buffer != NULL) && (transaction->rx_length > 0U);

    if (!has_write && !has_read)
    {
        return xRETURN_xERR_xI2C_INVALID_ARG;
    }

    uint32_t timeout_loops = transaction->timeout_ms * 10000U;
    if (timeout_loops == 0U)
    {
        timeout_loops = 1000000U;
    }

    ctx->is_busy    = true;

    // Wait for Bus Busy to be cleared
    uint32_t guard = 0U;
    while ((regs->IRQSTATUS_RAW & CSL_I2C_IRQSTATUS_RAW_BB_MASK) != 0U)
    {
        if (++guard >= timeout_loops)
        {
            ctx->is_busy    = false;
            return xRETURN_xERR_xI2C_TIMEOUT;
        }
    }

    regs->BUF |= CSL_I2C_BUF_TXFIFO_CLR_MASK;
    regs->BUF |= CSL_I2C_BUF_RXFIFO_CLR_MASK;

    xRETURN_t transfer_status = xRETURN_OK;

    // --- WRITE PHASE ---
    if (has_write)
    {
        regs->SA  = transaction->device_address;
        regs->CNT = transaction->tx_length;

        // MST | TRX | I2C_EN (Transmitter Mode)
        regs->CON = I2C_CON_MST_MASK | I2C_CON_TRX_MASK | I2C_CON_I2C_EN_MASK;

        // Generate START
        regs->CON |= I2C_CON_STT_MASK;

        for (uint32_t i = 0U; i < transaction->tx_length; i++)
        {
            guard = 0U;
            while (1)
            {
                uint32_t raw_stat = regs->IRQSTATUS_RAW;

                if ((raw_stat & CSL_I2C_IRQSTATUS_NACK_MASK) != 0U)
                {
                    regs->IRQSTATUS = CSL_I2C_IRQSTATUS_NACK_MASK;
                    transfer_status = xRETURN_xERR_xI2C_NACK;
                    break;
                }
                if ((raw_stat & CSL_I2C_IRQSTATUS_AL_MASK) != 0U)
                {
                    regs->IRQSTATUS = CSL_I2C_IRQSTATUS_AL_MASK;
                    transfer_status = xRETURN_xERR_xI2C_ARBITRATION_LOST;
                    break;
                }
                if ((raw_stat & CSL_I2C_IRQSTATUS_AERR_MASK) != 0U)
                {
                    regs->IRQSTATUS = CSL_I2C_IRQSTATUS_AERR_MASK;
                    transfer_status = xRETURN_xERR_xI2C_BUS_ERROR;
                    break;
                }
                if ((raw_stat & CSL_I2C_IRQSTATUS_XRDY_MASK) != 0U)
                {
                    break;
                }

                if (++guard >= timeout_loops)
                {
                    transfer_status = xRETURN_xERR_xI2C_TIMEOUT;
                    break;
                }
            }

            if (transfer_status != xRETURN_OK)
            {
                break;
            }

            regs->DATA      = (uint32_t)transaction->tx_buffer[i];
            regs->IRQSTATUS = CSL_I2C_IRQSTATUS_XRDY_MASK;
        }

        if (transfer_status == xRETURN_OK)
        {
            // Wait for Register Access Ready (ARDY)
            guard = 0U;
            while ((regs->IRQSTATUS_RAW & CSL_I2C_IRQSTATUS_ARDY_MASK) == 0U)
            {
                if (++guard >= timeout_loops)
                {
                    transfer_status = xRETURN_xERR_xI2C_TIMEOUT;
                    break;
                }
            }
            regs->IRQSTATUS = CSL_I2C_IRQSTATUS_ARDY_MASK;
        }

        if (!has_read || (transfer_status != xRETURN_OK))
        {
            // Generate STOP condition
            regs->CON |= I2C_CON_STP_MASK;

            // Wait for Bus Free (BF) or STOP condition to complete
            guard = 0U;
            while ((regs->IRQSTATUS_RAW & CSL_I2C_IRQSTATUS_BF_MASK) == 0U)
            {
                if (++guard >= timeout_loops)
                {
                    if (transfer_status == xRETURN_OK)
                    {
                        transfer_status = xRETURN_xERR_xI2C_TIMEOUT;
                    }
                    break;
                }
            }
            regs->IRQSTATUS = CSL_I2C_IRQSTATUS_BF_MASK;
        }
    }

    // --- READ PHASE ---
    if ((transfer_status == xRETURN_OK) && has_read)
    {
        regs->SA  = transaction->device_address;
        regs->CNT = transaction->rx_length;

        // MST | I2C_EN (Receiver Mode, TRX bit = 0)
        regs->CON = I2C_CON_MST_MASK | I2C_CON_I2C_EN_MASK;

        // Generate START (repeated start if has_write was true)
        regs->CON |= I2C_CON_STT_MASK;

        for (uint32_t i = 0U; i < transaction->rx_length; i++)
        {
            guard = 0U;
            while (1)
            {
                uint32_t raw_stat = regs->IRQSTATUS_RAW;

                if ((raw_stat & CSL_I2C_IRQSTATUS_NACK_MASK) != 0U)
                {
                    regs->IRQSTATUS = CSL_I2C_IRQSTATUS_NACK_MASK;
                    transfer_status = xRETURN_xERR_xI2C_NACK;
                    break;
                }
                if ((raw_stat & CSL_I2C_IRQSTATUS_AL_MASK) != 0U)
                {
                    regs->IRQSTATUS = CSL_I2C_IRQSTATUS_AL_MASK;
                    transfer_status = xRETURN_xERR_xI2C_ARBITRATION_LOST;
                    break;
                }
                if ((raw_stat & CSL_I2C_IRQSTATUS_AERR_MASK) != 0U)
                {
                    regs->IRQSTATUS = CSL_I2C_IRQSTATUS_AERR_MASK;
                    transfer_status = xRETURN_xERR_xI2C_BUS_ERROR;
                    break;
                }
                if ((raw_stat & CSL_I2C_IRQSTATUS_RRDY_MASK) != 0U)
                {
                    break;
                }

                if (++guard >= timeout_loops)
                {
                    transfer_status = xRETURN_xERR_xI2C_TIMEOUT;
                    break;
                }
            }

            if (transfer_status != xRETURN_OK)
            {
                break;
            }

            transaction->rx_buffer[i] = (uint8_t)regs->DATA;
            regs->IRQSTATUS           = CSL_I2C_IRQSTATUS_RRDY_MASK;
        }

        // Generate STOP condition
        regs->CON |= I2C_CON_STP_MASK;

        // Wait for Bus Free (BF) or STOP condition to complete
        guard = 0U;
        while ((regs->IRQSTATUS_RAW & CSL_I2C_IRQSTATUS_BF_MASK) == 0U)
        {
            if (++guard >= timeout_loops)
            {
                if (transfer_status == xRETURN_OK)
                {
                    transfer_status = xRETURN_xERR_xI2C_TIMEOUT;
                }
                break;
            }
        }
        regs->IRQSTATUS = CSL_I2C_IRQSTATUS_BF_MASK;
    }

    // Clean up all raw status flags
    regs->IRQSTATUS = 0x7FFFU;

    ctx->is_busy    = false;

    return transfer_status;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// EOF /////////////////////////////////////////////////////////////////////////////
