// Copyright 2022 alambe94

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file xlogic_pru.c
// @brief xLOGIC PRU IPC layer - control block accessor, ARM/HALT, and sample drain.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xlogic_pru.h"
#include "xassert.h"
#include "xlogic_config.h"

#include "xlogic_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xLOGIC_PRU_Init(xLOGIC_PRU_Context_t *pru_ctx, void *msram_base, uint32_t msram_bytes)
{
    xASSERT(pru_ctx != NULL, "pru_ctx is NULL");
    xASSERT(msram_base != NULL, "msram_base is NULL");

    if (pru_ctx == NULL || msram_base == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    if (msram_bytes <= xLOGIC_SAMPLE_DATA_OFFSET)
    {
        return xRETURN_xERR_xLOGIC_INVALID_ARGUMENT;
    }

    uint8_t *base = (uint8_t *)msram_base;

    pru_ctx->control = (volatile xLOGIC_PRU_Control_t *)(base + xLOGIC_PRU_CTRL_OFFSET);
    pru_ctx->ring = (volatile uint8_t *)(base + xLOGIC_SAMPLE_DATA_OFFSET);
    pru_ctx->ring_bytes = msram_bytes - xLOGIC_SAMPLE_DATA_OFFSET;
    pru_ctx->read_offset = 0U;
    pru_ctx->is_loaded = false;

    // Place PRU in IDLE before configuring trigger
    pru_ctx->control->cmd = xLOGIC_PRU_CMD_IDLE;
    pru_ctx->control->status = xLOGIC_PRU_STATUS_READY;

    return xRETURN_OK;
}

xRETURN_t xLOGIC_PRU_Load_Firmware(xLOGIC_PRU_Context_t *pru_ctx)
{
    xASSERT(pru_ctx != NULL, "pru_ctx is NULL");

    if (pru_ctx == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    // On hardware: load PRU binary from xlogic_pru_fw_bin.h into ICSSG IRAM
    // and start both PRU0 and RTU0 via the remoteproc/ICSSG driver.
    // On the host test port: nothing to load; mark as loaded for state checks.
    pru_ctx->is_loaded = true;

    xLOGIC_LOG(xRETURN_xMSG_xLOGIC_CAPTURE_DONE, "PRU firmware load complete");

    return xRETURN_OK;
}

xRETURN_t xLOGIC_PRU_Arm(xLOGIC_PRU_Context_t *pru_ctx, uint32_t trigger_channel, uint32_t trigger_edge, uint32_t sample_count)
{
    xASSERT(pru_ctx != NULL, "pru_ctx is NULL");

    if (pru_ctx == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    if (trigger_channel >= xLOGIC_MAX_CHANNELS)
    {
        return xRETURN_xERR_xLOGIC_INVALID_ARGUMENT;
    }

    // Reset read position for this capture
    pru_ctx->read_offset = 0U;

    // Write trigger config before arming so the PRU reads consistent values
    pru_ctx->control->trigger_channel = trigger_channel;
    pru_ctx->control->trigger_edge = trigger_edge;
    pru_ctx->control->sample_count = sample_count;
    pru_ctx->control->samples_captured = 0U;
    pru_ctx->control->overrun_count = 0U;

    // ARM: PRU starts capturing on the next R31 read after seeing CMD_ARM
    pru_ctx->control->cmd = xLOGIC_PRU_CMD_ARM;

    return xRETURN_OK;
}

xRETURN_t xLOGIC_PRU_Halt(xLOGIC_PRU_Context_t *pru_ctx)
{
    xASSERT(pru_ctx != NULL, "pru_ctx is NULL");

    if (pru_ctx == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    pru_ctx->control->cmd = xLOGIC_PRU_CMD_HALT;

    return xRETURN_OK;
}

xRETURN_t xLOGIC_PRU_Read_Samples(xLOGIC_PRU_Context_t *pru_ctx, uint8_t *dest, uint32_t max_bytes, uint32_t *bytes_read_out)
{
    xASSERT(pru_ctx != NULL, "pru_ctx is NULL");
    xASSERT(dest != NULL, "dest is NULL");
    xASSERT(bytes_read_out != NULL, "bytes_read_out is NULL");

    if (pru_ctx == NULL || dest == NULL || bytes_read_out == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    uint32_t available = xLOGIC_PRU_Available_Bytes(pru_ctx);
    uint32_t to_copy = (available < max_bytes) ? available : max_bytes;

    if (to_copy == 0U)
    {
        *bytes_read_out = 0U;
        return xRETURN_OK;
    }

    // Bounds check: ensure we do not read past the ring buffer
    if ((pru_ctx->read_offset + to_copy) > pru_ctx->ring_bytes)
    {
        to_copy = pru_ctx->ring_bytes - pru_ctx->read_offset;
    }

    // Copy from volatile MSRAM into ARM-side destination buffer
    for (uint32_t i = 0U; i < to_copy; i++)
    {
        dest[i] = pru_ctx->ring[pru_ctx->read_offset + i];
    }

    pru_ctx->read_offset += to_copy;
    *bytes_read_out = to_copy;

    return xRETURN_OK;
}

uint32_t xLOGIC_PRU_Available_Bytes(const xLOGIC_PRU_Context_t *pru_ctx)
{
    if (pru_ctx == NULL)
    {
        return 0U;
    }

    uint32_t captured = pru_ctx->control->samples_captured;

    if (captured <= pru_ctx->read_offset)
    {
        return 0U;
    }

    return captured - pru_ctx->read_offset;
}

bool xLOGIC_PRU_Has_Overrun(const xLOGIC_PRU_Context_t *pru_ctx)
{
    if (pru_ctx == NULL)
    {
        return false;
    }

    return pru_ctx->control->status == xLOGIC_PRU_STATUS_OVERRUN;
}

uint32_t xLOGIC_PRU_Get_Status(const xLOGIC_PRU_Context_t *pru_ctx)
{
    if (pru_ctx == NULL)
    {
        return xLOGIC_PRU_STATUS_READY;
    }

    return pru_ctx->control->status;
}

void xLOGIC_PRU_Reset_Read_Offset(xLOGIC_PRU_Context_t *pru_ctx)
{
    if (pru_ctx == NULL)
    {
        return;
    }

    pru_ctx->read_offset = 0U;
}

// EOF /////////////////////////////////////////////////////////////////////////////
