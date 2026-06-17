// Copyright 2022 alambe94
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
// @brief TI AM243x hardware port implementation for the xSPI driver core.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xassert.h"
#include "xspi_drv.h"
#include "xspi_log.h"

// CSL INCLUDES
#include <drivers/hw_include/cslr_mcspi.h>

// MACROS //////////////////////////////////////////////////////////////////////////
#define REG32_RD(addr)          (*(volatile uint32_t *)(addr))
#define REG32_WR(addr, val)     (*(volatile uint32_t *)(addr) = (uint32_t)(val))

#define MCSPI_CHCONF(ch)        (0x12CU + ((ch) * 0x14U))
#define MCSPI_CHSTAT(ch)        (0x130U + ((ch) * 0x14U))
#define MCSPI_CHCTRL(ch)        (0x134U + ((ch) * 0x14U))
#define MCSPI_CHTX(ch)          (0x138U + ((ch) * 0x14U))
#define MCSPI_CHRX(ch)          (0x13CU + ((ch) * 0x14U))

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static void reg32_fins(uint32_t addr, uint32_t mask, uint32_t shift, uint32_t val);
static void set_clk_config(uint32_t base_addr, uint32_t ch, uint32_t input_clk, uint32_t bitrate);
static xRETURN_t mcspi_reset(uint32_t base_addr);
static xRETURN_t am243x_init(void *driver_ctx, const xSPI_Config_t *config);
static xRETURN_t am243x_deinit(void *driver_ctx);
static xRETURN_t am243x_start(void *driver_ctx);
static xRETURN_t am243x_stop(void *driver_ctx);
static xRETURN_t am243x_transfer(void *driver_ctx, const xSPI_Device_t *device, const xSPI_Transaction_t *transaction);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static void reg32_fins(uint32_t addr, uint32_t mask, uint32_t shift, uint32_t val)
{
    uint32_t reg = REG32_RD(addr);
    reg &= ~mask;
    reg |= (val << shift) & mask;
    REG32_WR(addr, reg);
}

static void set_clk_config(uint32_t base_addr, uint32_t ch, uint32_t input_clk, uint32_t bitrate)
{
    uint32_t fRatio;
    uint32_t clkD;
    uint32_t extClk;

    fRatio = input_clk / bitrate;
    if (((input_clk % bitrate) != 0U) && (fRatio < 4096U))
    {
        fRatio++;
    }
    if (fRatio == 0U)
    {
        fRatio = 1U;
    }

    uint32_t conf_addr = base_addr + MCSPI_CHCONF(ch);
    uint32_t ctrl_addr = base_addr + MCSPI_CHCTRL(ch);

    if ((fRatio & (fRatio - 1U)) != 0U)
    {
        // Set granularity to 1 clock cycle (CLKG = 1) (bit 29)
        reg32_fins(conf_addr, 1U << 29U, 29U, 1U);

        extClk = (fRatio - 1U) >> 4U;
        clkD   = (fRatio - 1U) & 0xFUL;

        // Set extClk
        reg32_fins(ctrl_addr, CSL_MCSPI_CH0CTRL_EXTCLK_MASK, CSL_MCSPI_CH0CTRL_EXTCLK_SHIFT, extClk);
    }
    else
    {
        // Clock granularity power of 2 (CLKG = 0)
        reg32_fins(conf_addr, 1U << 29U, 29U, 0U);

        clkD = 0U;
        while (1U != fRatio)
        {
            fRatio >>= 1U;
            clkD++;
        }
    }

    // Configure CLKD (bits 2-5)
    reg32_fins(conf_addr, CSL_MCSPI_CH0CONF_CLKD_MASK, CSL_MCSPI_CH0CONF_CLKD_SHIFT, clkD);
}

static xRETURN_t mcspi_reset(uint32_t base_addr)
{
    uint32_t regVal;
    uint32_t timeout;

    // Set SOFTRESET in CSL_MCSPI_SYSCONFIG
    reg32_fins(base_addr + CSL_MCSPI_SYSCONFIG,
               CSL_MCSPI_SYSCONFIG_SOFTRESET_MASK,
               CSL_MCSPI_SYSCONFIG_SOFTRESET_SHIFT,
               CSL_MCSPI_SYSCONFIG_SOFTRESET_ON);

    timeout = 100000U;
    while (timeout > 0U)
    {
        regVal = REG32_RD(base_addr + CSL_MCSPI_SYSSTATUS);
        if ((regVal & CSL_MCSPI_SYSSTATUS_RESETDONE_MASK) == CSL_MCSPI_SYSSTATUS_RESETDONE_MASK)
        {
            break;
        }
        timeout--;
    }

    if (timeout == 0U)
    {
        return xRETURN_xERR_xSPI_TIMEOUT;
    }

    return xRETURN_OK;
}


static xRETURN_t am243x_init(void *driver_ctx, const xSPI_Config_t *config)
{
    xSPI_AM243x_Context_t *port_ctx = (xSPI_AM243x_Context_t *)driver_ctx;
    xRETURN_t status;

    if ((port_ctx == NULL) || (config == NULL))
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (port_ctx->is_initialized == true)
    {
        return xRETURN_xERR_xSPI_INVALID_STATE;
    }

    status = mcspi_reset(port_ctx->base_addr);
    if (status != xRETURN_OK)
    {
        return status;
    }

    // Configure CSL_MCSPI_SYSCONFIG: No Idle, clock activity both, auto idle off
    uint32_t sysconfig_val = (((uint32_t)CSL_MCSPI_SYSCONFIG_CLOCKACTIVITY_BOTH << CSL_MCSPI_SYSCONFIG_CLOCKACTIVITY_SHIFT) |
                              ((uint32_t)CSL_MCSPI_SYSCONFIG_SIDLEMODE_NO << CSL_MCSPI_SYSCONFIG_SIDLEMODE_SHIFT) |
                              ((uint32_t)CSL_MCSPI_SYSCONFIG_ENAWAKEUP_NOWAKEUP << CSL_MCSPI_SYSCONFIG_ENAWAKEUP_SHIFT) |
                              ((uint32_t)CSL_MCSPI_SYSCONFIG_AUTOIDLE_OFF << CSL_MCSPI_SYSCONFIG_AUTOIDLE_SHIFT));
    REG32_WR(port_ctx->base_addr + CSL_MCSPI_SYSCONFIG, sysconfig_val);

    // Configure CSL_MCSPI_MODULCTRL: Controller mode (MS = 0), Single channel (SINGLE = 1), 4-pin mode (PIN34 = 0)
    uint32_t modulctrl = (((uint32_t)CSL_MCSPI_MODULCTRL_MS_MASTER << CSL_MCSPI_MODULCTRL_MS_SHIFT) |
                          ((uint32_t)CSL_MCSPI_MODULCTRL_SINGLE_SINGLE << CSL_MCSPI_MODULCTRL_SINGLE_SHIFT) |
                          ((uint32_t)CSL_MCSPI_MODULCTRL_PIN34_4PINMODE << CSL_MCSPI_MODULCTRL_PIN34_SHIFT));
    REG32_WR(port_ctx->base_addr + CSL_MCSPI_MODULCTRL, modulctrl);

    port_ctx->is_initialized = true;

    return xRETURN_OK;
}

static xRETURN_t am243x_deinit(void *driver_ctx)
{
    xSPI_AM243x_Context_t *port_ctx = (xSPI_AM243x_Context_t *)driver_ctx;
    xRETURN_t status;

    if (port_ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (port_ctx->is_initialized == false)
    {
        return xRETURN_xERR_xSPI_NOT_INITIALIZED;
    }

    if (port_ctx->is_busy == true)
    {
        return xRETURN_xERR_xSPI_BUSY;
    }

    status = mcspi_reset(port_ctx->base_addr);
    if (status != xRETURN_OK)
    {
        return status;
    }

    port_ctx->is_initialized = false;
    port_ctx->is_started = false;

    return xRETURN_OK;
}

static xRETURN_t am243x_start(void *driver_ctx)
{
    xSPI_AM243x_Context_t *port_ctx = (xSPI_AM243x_Context_t *)driver_ctx;

    if (port_ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (port_ctx->is_initialized == false)
    {
        return xRETURN_xERR_xSPI_NOT_INITIALIZED;
    }

    if (port_ctx->is_started == true)
    {
        return xRETURN_OK;
    }

    port_ctx->is_started = true;

    return xRETURN_OK;
}

static xRETURN_t am243x_stop(void *driver_ctx)
{
    xSPI_AM243x_Context_t *port_ctx = (xSPI_AM243x_Context_t *)driver_ctx;

    if (port_ctx == NULL)
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (port_ctx->is_initialized == false)
    {
        return xRETURN_xERR_xSPI_NOT_INITIALIZED;
    }

    if (port_ctx->is_busy == true)
    {
        return xRETURN_xERR_xSPI_BUSY;
    }

    if (port_ctx->is_started == false)
    {
        return xRETURN_OK;
    }

    // Disable all channels (0-3)
    for (uint32_t ch = 0; ch < 4U; ch++)
    {
        REG32_WR(port_ctx->base_addr + MCSPI_CHCTRL(ch), 0U);
    }

    port_ctx->is_started = false;

    return xRETURN_OK;
}


static xRETURN_t am243x_transfer(void *driver_ctx, const xSPI_Device_t *device, const xSPI_Transaction_t *transaction)
{
    xSPI_AM243x_Context_t *port_ctx = (xSPI_AM243x_Context_t *)driver_ctx;
    xRETURN_t status = xRETURN_OK;

    if ((port_ctx == NULL) || (device == NULL) || (transaction == NULL))
    {
        return xRETURN_xERR_xSPI_NULL_POINTER;
    }

    if (port_ctx->is_started == false)
    {
        return xRETURN_xERR_xSPI_NOT_STARTED;
    }

    if (port_ctx->is_busy == true)
    {
        return xRETURN_xERR_xSPI_BUSY;
    }

    if (device->chip_select >= 4U)
    {
        return xRETURN_xERR_xSPI_INVALID_ARG;
    }

    port_ctx->is_busy = true;

    uint32_t base_addr = port_ctx->base_addr;
    uint32_t ch = device->chip_select;

    // 1. Configure Clock
    set_clk_config(base_addr, ch, port_ctx->input_clock_hz, transaction->clock_hz);

    // 2. Configure Channel parameters in CHxCONF
    uint32_t conf_val = REG32_RD(base_addr + MCSPI_CHCONF(ch));

    // Clear POL/PHA, EPOL, TRM, DPE0/DPE1/IS, WL
    conf_val &= ~((1U << 0U) | (1U << 1U) | (1U << 6U) | (3U << 12U) | (7U << 16U) | (0x1FU << 7U));

    // Configure CPOL / CPHA
    if ((device->mode_flags & xSPI_MODE_CPOL_HIGH) != 0UL)
    {
        conf_val |= (1U << 1U); // Set POL
    }
    if ((device->mode_flags & xSPI_MODE_CPHA_SECOND_EDGE) != 0UL)
    {
        conf_val |= (1U << 0U); // Set PHA
    }

    // CS Polarity: 1 = active high, 0 = active low
    if ((device->mode_flags & xSPI_MODE_CS_ACTIVE_HIGH) != 0UL)
    {
        conf_val |= (1U << 6U); // Set EPOL
    }

    // Word length
    uint32_t wl = (uint32_t)transaction->bits_per_word - 1U;
    conf_val |= (wl << 7U);

    // Pin routing: SPIDAT0 = MOSI (output, DPE0=0), SPIDAT1 = MISO (input, DPE1=1, IS=1)
    conf_val |= (1U << 17U); // DPE1 = 1 (disable transmission on line 1)
    conf_val |= (1U << 18U); // IS = 1 (line 1 is input)
    // Note: DPE0 (bit 16) is left as 0 (enable transmission on line 0)

    REG32_WR(base_addr + MCSPI_CHCONF(ch), conf_val);

    // 3. Configure loopback if needed
    uint32_t modulctrl = REG32_RD(base_addr + CSL_MCSPI_MODULCTRL);
    modulctrl &= ~CSL_MCSPI_MODULCTRL_SYSTEM_TEST_MASK;
    if ((device->mode_flags & xSPI_MODE_LOOPBACK) != 0UL)
    {
        modulctrl |= CSL_MCSPI_MODULCTRL_SYSTEM_TEST_MASK;
    }
    REG32_WR(base_addr + CSL_MCSPI_MODULCTRL, modulctrl);

    // 4. Perform polling transfer
    uint32_t tx_idx = 0;
    uint32_t rx_idx = 0;
    uint32_t len = transaction->length;
    const uint8_t *tx_buf = transaction->tx_buffer;
    uint8_t *rx_buf = transaction->rx_buffer;

    // Enable the channel
    REG32_WR(base_addr + MCSPI_CHCTRL(ch), REG32_RD(base_addr + MCSPI_CHCTRL(ch)) | 1U);

    // Assert CS manually (FORCE bit)
    REG32_WR(base_addr + MCSPI_CHCONF(ch), REG32_RD(base_addr + MCSPI_CHCONF(ch)) | (1U << 20U));

    uint32_t timeout_guard = 1000000U * len;
    while (((tx_idx < len) || (rx_idx < len)) && (timeout_guard > 0U))
    {
        uint32_t stat = REG32_RD(base_addr + MCSPI_CHSTAT(ch));

        // TX empty status (bit 1)
        if ((tx_idx < len) && ((stat & (1U << 1U)) != 0U))
        {
            uint32_t tx_data = 0xFFU;
            if (tx_buf != NULL)
            {
                tx_data = tx_buf[tx_idx];
            }
            REG32_WR(base_addr + MCSPI_CHTX(ch), tx_data);
            tx_idx++;
        }

        // RX full status (bit 0)
        if ((rx_idx < len) && ((stat & (1U << 0U)) != 0U))
        {
            uint32_t rx_data = REG32_RD(base_addr + MCSPI_CHRX(ch));
            if (rx_buf != NULL)
            {
                rx_buf[rx_idx] = (uint8_t)rx_data;
            }
            rx_idx++;
        }

        timeout_guard--;
    }

    // Deassert CS
    REG32_WR(base_addr + MCSPI_CHCONF(ch), REG32_RD(base_addr + MCSPI_CHCONF(ch)) & ~(1U << 20U));

    // Disable the channel
    REG32_WR(base_addr + MCSPI_CHCTRL(ch), REG32_RD(base_addr + MCSPI_CHCTRL(ch)) & ~1U);

    if (timeout_guard == 0U)
    {
        status = xRETURN_xERR_xSPI_TIMEOUT;
    }

    port_ctx->is_busy = false;
    port_ctx->last_error = status;

    return status;
}


// PUBLIC VARIABLES ////////////////////////////////////////////////////////////////

const xSPI_Driver_Ops_t xSPI_AM243x_Driver_Ops = {
    .init = am243x_init,
    .deinit = am243x_deinit,
    .start = am243x_start,
    .stop = am243x_stop,
    .transfer = am243x_transfer,
};

// EOF /////////////////////////////////////////////////////////////////////////////
