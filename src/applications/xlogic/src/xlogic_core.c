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

// @file xlogic_core.c
// @brief xLOGIC core state machine - Arm, Abort, Poll, and state accessors.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xlogic_core.h"
#include "xassert.h"
#include "xlogic_config.h"

#include "xlogic_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static xRETURN_t poll_armed(xLOGIC_Core_Context_t *core_ctx);
static xRETURN_t poll_done(xLOGIC_Core_Context_t *core_ctx);
static uint32_t clamp_sample_rate(uint32_t rate);
static uint32_t map_trigger_channel(uint32_t trigger_mask);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// Clamp a requested sample rate to [xLOGIC_MIN_SAMPLE_RATE, xLOGIC_MAX_SAMPLE_RATE].
static uint32_t clamp_sample_rate(uint32_t rate)
{
    if (rate > xLOGIC_MAX_SAMPLE_RATE)
    {
        return xLOGIC_MAX_SAMPLE_RATE;
    }

    if (rate < xLOGIC_MIN_SAMPLE_RATE)
    {
        return xLOGIC_MIN_SAMPLE_RATE;
    }

    return rate;
}

// Extract the PRU trigger channel index from a multi-channel SUMP trigger_mask.
// Uses the lowest set bit as the single hardware trigger channel.
static uint32_t map_trigger_channel(uint32_t trigger_mask)
{
    for (uint32_t i = 0U; i < xLOGIC_MAX_CHANNELS; i++)
    {
        if ((trigger_mask & (1U << i)) != 0U)
        {
            return i;
        }
    }

    return 0U; // fallback: channel 0
}

// Handle state machine while ARMED: check PRU status, transition on done/overrun.
static xRETURN_t poll_armed(xLOGIC_Core_Context_t *core_ctx)
{
    uint32_t pru_status = xLOGIC_PRU_Get_Status(core_ctx->pru_ctx);

    if (pru_status == xLOGIC_PRU_STATUS_OVERRUN)
    {
        xLOGIC_LOG(xRETURN_xERR_xLOGIC_OVERRUN, "PRU overrun while ARMED");
        core_ctx->state = xLOGIC_STATE_ERROR;
        return xRETURN_xERR_xLOGIC_OVERRUN;
    }

    if (pru_status == xLOGIC_PRU_STATUS_DONE)
    {
        xLOGIC_LOG(xRETURN_xMSG_xLOGIC_CAPTURE_DONE, "PRU done; transitioning to DONE");
        core_ctx->state = xLOGIC_STATE_DONE;
    }

    return xRETURN_OK;
}

// Handle state machine while DONE: drain samples from PRU ring and send via transport.
static xRETURN_t poll_done(xLOGIC_Core_Context_t *core_ctx)
{
    uint32_t bytes_read = 0U;
    uint32_t space = core_ctx->sample_buffer_bytes - core_ctx->samples_captured;

    xRETURN_t ret = xLOGIC_PRU_Read_Samples(core_ctx->pru_ctx, core_ctx->sample_buffer + core_ctx->samples_captured, space, &bytes_read);
    if (ret != xRETURN_OK)
    {
        return ret;
    }

    core_ctx->samples_captured += bytes_read;

    // Send all captured samples to the host via the transport layer (SUMP reversed)
    ret = xLOGIC_Transport_Send_Samples(core_ctx->transport_ctx, core_ctx->sample_buffer, core_ctx->samples_captured);
    if (ret != xRETURN_OK)
    {
        xLOGIC_LOG(xRETURN_xERR_xLOGIC_TX_FAILED, "sample send failed");
        return ret;
    }

    xLOGIC_LOG(xRETURN_xMSG_xLOGIC_CAPTURE_DONE, "samples sent; returning to IDLE");
    core_ctx->state = xLOGIC_STATE_IDLE;
    core_ctx->samples_captured = 0U;

    return xRETURN_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xLOGIC_Init(xLOGIC_Core_Context_t *core_ctx,
                      xLOGIC_PRU_Context_t *pru_ctx,
                      xLOGIC_Transport_Context_t *transport_ctx,
                      uint8_t *sample_buffer,
                      uint32_t sample_buffer_bytes)
{
    xASSERT(core_ctx != NULL, "core_ctx is NULL");
    xASSERT(pru_ctx != NULL, "pru_ctx is NULL");
    xASSERT(transport_ctx != NULL, "transport_ctx is NULL");
    xASSERT(sample_buffer != NULL, "sample_buffer is NULL");

    if (core_ctx == NULL || pru_ctx == NULL || transport_ctx == NULL || sample_buffer == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    if (sample_buffer_bytes == 0U)
    {
        return xRETURN_xERR_xLOGIC_INVALID_ARGUMENT;
    }

    core_ctx->state = xLOGIC_STATE_IDLE;
    core_ctx->pru_ctx = pru_ctx;
    core_ctx->transport_ctx = transport_ctx;
    core_ctx->sample_buffer = sample_buffer;
    core_ctx->sample_buffer_bytes = sample_buffer_bytes;
    core_ctx->samples_captured = 0U;

    (void)memset(&core_ctx->config, 0, sizeof(core_ctx->config));

    return xRETURN_OK;
}

xRETURN_t xLOGIC_Arm(xLOGIC_Core_Context_t *core_ctx, const xLOGIC_Config_t *config)
{
    xASSERT(core_ctx != NULL, "core_ctx is NULL");
    xASSERT(config != NULL, "config is NULL");

    if (core_ctx == NULL || config == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    if (core_ctx->state != xLOGIC_STATE_IDLE)
    {
        xLOGIC_LOG(xRETURN_xERR_xLOGIC_INVALID_STATE, "Arm called while not IDLE");
        return xRETURN_xERR_xLOGIC_INVALID_STATE;
    }

    // Copy and validate config
    core_ctx->config = *config;
    core_ctx->config.sample_rate = clamp_sample_rate(config->sample_rate);
    core_ctx->samples_captured = 0U;

    // Derive PRU trigger channel from SUMP multi-channel trigger_mask
    uint32_t trigger_ch = 0U;
    uint32_t trigger_edge = xLOGIC_TRIGGER_EDGE_RISING;

    if (config->trigger_mode != xLOGIC_TRIGGER_NONE && config->trigger_mask != 0U)
    {
        trigger_ch = map_trigger_channel(config->trigger_mask);
        trigger_edge = config->trigger_edge;
    }

    xRETURN_t ret = xLOGIC_PRU_Arm(core_ctx->pru_ctx, trigger_ch, trigger_edge, config->sample_count);
    if (ret != xRETURN_OK)
    {
        xLOGIC_LOG(xRETURN_xERR_xLOGIC_INVALID_STATE, "PRU arm failed");
        return ret;
    }

    core_ctx->state = xLOGIC_STATE_ARMED;
    xLOGIC_Transport_Clear_Requests(core_ctx->transport_ctx);

    xLOGIC_LOG(xRETURN_xMSG_xLOGIC_CAPTURE_DONE, "core armed; PRU running");

    return xRETURN_OK;
}

xRETURN_t xLOGIC_Abort(xLOGIC_Core_Context_t *core_ctx)
{
    xASSERT(core_ctx != NULL, "core_ctx is NULL");

    if (core_ctx == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    // Halt the PRU regardless of current state; ignore PRU errors during abort
    if (core_ctx->pru_ctx != NULL)
    {
        (void)xLOGIC_PRU_Halt(core_ctx->pru_ctx);
    }

    if (core_ctx->transport_ctx != NULL)
    {
        xLOGIC_Transport_Clear_Requests(core_ctx->transport_ctx);
    }

    core_ctx->state = xLOGIC_STATE_IDLE;
    core_ctx->samples_captured = 0U;

    xLOGIC_LOG(xRETURN_xMSG_xLOGIC_CAPTURE_DONE, "core aborted; returned to IDLE");

    return xRETURN_OK;
}

xRETURN_t xLOGIC_Poll(xLOGIC_Core_Context_t *core_ctx)
{
    xASSERT(core_ctx != NULL, "core_ctx is NULL");

    if (core_ctx == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    switch (core_ctx->state)
    {
    case xLOGIC_STATE_IDLE:
        return xRETURN_OK;

    case xLOGIC_STATE_ARMED:
    case xLOGIC_STATE_TRIGGERED:
    case xLOGIC_STATE_CAPTURING:
        return poll_armed(core_ctx);

    case xLOGIC_STATE_DONE:
        return poll_done(core_ctx);

    case xLOGIC_STATE_ERROR:
        // Attempt graceful halt; application must call xLOGIC_Abort to recover
        (void)xLOGIC_PRU_Halt(core_ctx->pru_ctx);
        return xRETURN_xERR_xLOGIC_INVALID_STATE;

    default:
        return xRETURN_xERR_xLOGIC_INVALID_STATE;
    }
}

xLOGIC_State_t xLOGIC_Get_State(const xLOGIC_Core_Context_t *core_ctx)
{
    if (core_ctx == NULL)
    {
        return xLOGIC_STATE_ERROR;
    }

    return core_ctx->state;
}

// EOF /////////////////////////////////////////////////////////////////////////////
