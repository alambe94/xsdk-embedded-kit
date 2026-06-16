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

// @file xnet_interface_fake.c
// @brief Implements the fake network interface driver for host verification.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xnet_interface_fake.h"
#include "xnet_defs.h"
#include "xnet_return.h"
#include "xassert.h"
#include "xnet_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t fake_interface_transmit(void *driver_ctx, const uint8_t *packet, uint32_t length, uint32_t tx_flags);
static xRETURN_t fake_interface_poll(void *driver_ctx);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t fake_interface_transmit(void *driver_ctx, const uint8_t *packet, uint32_t length, uint32_t tx_flags)
{
    xASSERT(driver_ctx != NULL, "driver_ctx is NULL");
    xASSERT(packet != NULL, "packet is NULL");
    if ((driver_ctx == NULL) || (packet == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xNET_Fake_Interface_Context_t *fake_ctx = (xNET_Fake_Interface_Context_t *)driver_ctx;

    if (fake_ctx->inject_tx_fail)
    {
        return xRETURN_xERR_xNET_LINK_DOWN;
    }

    if (length > (xNET_ETHERNET_MTU + xNET_ETHERNET_HEADER_SIZE))
    {
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    if (fake_ctx->tx_count >= xNET_FAKE_TX_QUEUE_DEPTH)
    {
        return xRETURN_xERR_xNET_BUFFER_TOO_SMALL;
    }

    (void)memcpy(fake_ctx->tx_queue[fake_ctx->tx_write_idx], packet, length);
    fake_ctx->tx_lengths[fake_ctx->tx_write_idx] = length;
    fake_ctx->tx_write_idx = (fake_ctx->tx_write_idx + 1U) % xNET_FAKE_TX_QUEUE_DEPTH;
    fake_ctx->tx_count++;

    // Suppress unused warning for tx_flags
    (void)tx_flags;

    return xRETURN_xNET_OK;
}

static xRETURN_t fake_interface_poll(void *driver_ctx)
{
    xASSERT(driver_ctx != NULL, "driver_ctx is NULL");
    if (driver_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xNET_Fake_Interface_Context_t *fake_ctx = (xNET_Fake_Interface_Context_t *)driver_ctx;
    xNET_Interface_Context_t *interface_ctx = fake_ctx->interface_ctx;
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if (interface_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    uint32_t limit = fake_ctx->rx_count;
    for (uint32_t i = 0U; (i < limit) && (i < xNET_FAKE_RX_QUEUE_DEPTH); i++)
    {
        uint32_t read_idx = fake_ctx->rx_read_idx;
        uint8_t *frame = fake_ctx->rx_queue[read_idx];
        uint32_t length = fake_ctx->rx_lengths[read_idx];

        fake_ctx->rx_read_idx = (fake_ctx->rx_read_idx + 1U) % xNET_FAKE_RX_QUEUE_DEPTH;
        fake_ctx->rx_count--;

        xRETURN_t status = xNET_Interface_RX_Frame(interface_ctx, frame, length, xNET_RX_FLAG_NONE);
        if (status != xRETURN_xNET_OK)
        {
            return status;
        }
    }

    return xRETURN_xNET_OK;
}

static const xNET_Interface_Ops_t s_fake_ops = {.transmit = fake_interface_transmit,
                                                .poll = fake_interface_poll,
                                                .set_multicast_filter = NULL,
                                                .flush = NULL};

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

const xNET_Interface_Ops_t *xNET_Fake_Interface_Get_Ops(void)
{
    return &s_fake_ops;
}

xRETURN_t xNET_Fake_Interface_Init(xNET_Fake_Interface_Context_t *fake_ctx, xNET_Interface_Context_t *interface_ctx)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if ((fake_ctx == NULL) || (interface_ctx == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    (void)memset(fake_ctx, 0, sizeof(xNET_Fake_Interface_Context_t));

    fake_ctx->interface_ctx = interface_ctx;
    fake_ctx->is_link_up = false;

    interface_ctx->driver_ctx = fake_ctx;
    interface_ctx->ops = &s_fake_ops;

    return xRETURN_xNET_OK;
}

xRETURN_t xNET_Fake_Interface_RX(xNET_Interface_Context_t *interface_ctx, const uint8_t *packet, uint32_t length)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(packet != NULL, "packet is NULL");
    if ((interface_ctx == NULL) || (packet == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (length > (xNET_ETHERNET_MTU + xNET_ETHERNET_HEADER_SIZE))
    {
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    xNET_Fake_Interface_Context_t *fake_ctx = (xNET_Fake_Interface_Context_t *)interface_ctx->driver_ctx;
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");
    if (fake_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (fake_ctx->rx_count >= xNET_FAKE_RX_QUEUE_DEPTH)
    {
        return xRETURN_xERR_xNET_BUFFER_TOO_SMALL;
    }

    (void)memcpy(fake_ctx->rx_queue[fake_ctx->rx_write_idx], packet, length);
    fake_ctx->rx_lengths[fake_ctx->rx_write_idx] = length;
    fake_ctx->rx_write_idx = (fake_ctx->rx_write_idx + 1U) % xNET_FAKE_RX_QUEUE_DEPTH;
    fake_ctx->rx_count++;

    return xRETURN_xNET_OK;
}

xRETURN_t xNET_Fake_Interface_TX_Pop(xNET_Fake_Interface_Context_t *fake_ctx, uint8_t *packet, uint32_t packet_size, uint32_t *length)
{
    xASSERT(fake_ctx != NULL, "fake_ctx is NULL");
    xASSERT(packet != NULL, "packet is NULL");
    xASSERT(length != NULL, "length is NULL");
    if ((fake_ctx == NULL) || (packet == NULL) || (length == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (fake_ctx->tx_count == 0U)
    {
        return xRETURN_xERR_xNET_NOT_FOUND;
    }

    uint32_t read_idx = fake_ctx->tx_read_idx;
    uint32_t packet_len = fake_ctx->tx_lengths[read_idx];

    if (packet_size < packet_len)
    {
        return xRETURN_xERR_xNET_BUFFER_TOO_SMALL;
    }

    (void)memcpy(packet, fake_ctx->tx_queue[read_idx], packet_len);
    *length = packet_len;

    fake_ctx->tx_read_idx = (fake_ctx->tx_read_idx + 1U) % xNET_FAKE_TX_QUEUE_DEPTH;
    fake_ctx->tx_count--;

    return xRETURN_xNET_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
