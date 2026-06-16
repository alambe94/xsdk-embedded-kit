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

// @file xlogic_transport.c
// @brief xLOGIC SUMP/CDC transport layer - RX byte routing, TX, and sample streaming.

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stddef.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xlogic_transport.h"
#include "xassert.h"
#include "xlogic_config.h"

#include "xlogic_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////

// SUMP device ID string (4 bytes; no null terminator on wire)
static const uint8_t k_sump_id[xLOGIC_SUMP_ID_BYTES] = {'1', 'A', 'L', 'S'};

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

static void reverse_bytes(uint8_t *data, uint32_t length);
static xRETURN_t handle_event(xLOGIC_Transport_Context_t *transport_ctx, xLOGIC_SUMP_Event_t event);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// Reverse length bytes of data in-place (needed for SUMP newest-first ordering).
static void reverse_bytes(uint8_t *data, uint32_t length)
{
    if (length <= 1U)
    {
        return;
    }

    uint32_t left = 0U;
    uint32_t right = length - 1U;

    while (left < right)
    {
        uint8_t tmp = data[left];
        data[left] = data[right];
        data[right] = tmp;
        left++;
        right--;
    }
}

// Dispatch a completed SUMP event: send protocol responses and set request flags.
static xRETURN_t handle_event(xLOGIC_Transport_Context_t *transport_ctx, xLOGIC_SUMP_Event_t event)
{
    xRETURN_t ret = xRETURN_OK;

    switch (event)
    {
    case xLOGIC_SUMP_EVENT_RESET:
        transport_ctx->has_run_request = false;
        transport_ctx->has_reset_request = true;
        break;

    case xLOGIC_SUMP_EVENT_QUERY_ID:
        // Respond with the 4-byte SUMP device ID "1ALS"
        ret = xLOGIC_Transport_Send_Response(transport_ctx, k_sump_id, xLOGIC_SUMP_ID_BYTES);
        break;

    case xLOGIC_SUMP_EVENT_METADATA:
    {
        // Build and send the SUMP v2 metadata block
        uint8_t meta_buf[xLOGIC_CONFIG_METADATA_BUF_BYTES];
        uint32_t meta_len = 0U;

        xRETURN_t build_ret = xLOGIC_SUMP_Build_Metadata(meta_buf, sizeof(meta_buf), &meta_len);
        if (build_ret != xRETURN_OK)
        {
            xLOGIC_LOG(xRETURN_xERR_xLOGIC_BUFFER_FULL, "metadata buffer too small");
            ret = build_ret;
            break;
        }

        ret = xLOGIC_Transport_Send_Response(transport_ctx, meta_buf, meta_len);
        break;
    }

    case xLOGIC_SUMP_EVENT_RUN:
        transport_ctx->has_run_request = true;
        break;

    case xLOGIC_SUMP_EVENT_CONFIG_SET:
    case xLOGIC_SUMP_EVENT_NONE:
    default:
        break;
    }

    return ret;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xLOGIC_Transport_Init(xLOGIC_Transport_Context_t *transport_ctx, xLOGIC_TX_Fn_t tx_fn, void *tx_ctx)
{
    xASSERT(transport_ctx != NULL, "transport_ctx is NULL");
    xASSERT(tx_fn != NULL, "tx_fn is NULL");

    if (transport_ctx == NULL || tx_fn == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    xRETURN_t ret = xLOGIC_SUMP_Init(&transport_ctx->sump_ctx);

    if (ret != xRETURN_OK)
    {
        return ret;
    }

    transport_ctx->tx_fn = tx_fn;
    transport_ctx->tx_ctx = tx_ctx;
    transport_ctx->has_run_request = false;
    transport_ctx->has_reset_request = false;

    return xRETURN_OK;
}

xRETURN_t xLOGIC_Transport_Process_RX(xLOGIC_Transport_Context_t *transport_ctx, const uint8_t *data, uint32_t length)
{
    xASSERT(transport_ctx != NULL, "transport_ctx is NULL");
    xASSERT(data != NULL, "data is NULL");

    if (transport_ctx == NULL || data == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    for (uint32_t i = 0U; i < length; i++)
    {
        xLOGIC_SUMP_Event_t event = xLOGIC_SUMP_EVENT_NONE;

        xRETURN_t ret = xLOGIC_SUMP_Feed_Byte(&transport_ctx->sump_ctx, data[i], &event);
        if (ret != xRETURN_OK)
        {
            return ret;
        }

        if (event != xLOGIC_SUMP_EVENT_NONE)
        {
            ret = handle_event(transport_ctx, event);

            if (ret != xRETURN_OK)
            {
                xLOGIC_LOG(xRETURN_xERR_xLOGIC_TX_FAILED, "transport event handling failed");
                return ret;
            }
        }
    }

    return xRETURN_OK;
}

xRETURN_t xLOGIC_Transport_Send_Response(xLOGIC_Transport_Context_t *transport_ctx, const uint8_t *data, uint32_t length)
{
    xASSERT(transport_ctx != NULL, "transport_ctx is NULL");
    xASSERT(data != NULL, "data is NULL");

    if (transport_ctx == NULL || data == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    if (length == 0U)
    {
        return xRETURN_OK;
    }

    xRETURN_t ret = transport_ctx->tx_fn(transport_ctx->tx_ctx, data, length);

    if (ret != xRETURN_OK)
    {
        xLOGIC_LOG(xRETURN_xERR_xLOGIC_TX_FAILED, "TX failed in Send_Response");
    }

    return ret;
}

xRETURN_t xLOGIC_Transport_Send_Samples(xLOGIC_Transport_Context_t *transport_ctx, uint8_t *samples, uint32_t sample_bytes)
{
    xASSERT(transport_ctx != NULL, "transport_ctx is NULL");
    xASSERT(samples != NULL, "samples is NULL");

    if (transport_ctx == NULL || samples == NULL)
    {
        return xRETURN_xERR_xLOGIC_NULL_POINTER;
    }

    if (sample_bytes == 0U)
    {
        return xRETURN_OK;
    }

    // SUMP expects samples in reverse capture order (newest first).
    // Reverse the staging buffer in-place before transmitting.
    reverse_bytes(samples, sample_bytes);

    xRETURN_t ret = transport_ctx->tx_fn(transport_ctx->tx_ctx, samples, sample_bytes);

    if (ret != xRETURN_OK)
    {
        xLOGIC_LOG(xRETURN_xERR_xLOGIC_TX_FAILED, "TX failed in Send_Samples");
    }

    return ret;
}

void xLOGIC_Transport_Clear_Requests(xLOGIC_Transport_Context_t *transport_ctx)
{
    if (transport_ctx == NULL)
    {
        return;
    }

    transport_ctx->has_run_request = false;
    transport_ctx->has_reset_request = false;
}

// EOF /////////////////////////////////////////////////////////////////////////////
