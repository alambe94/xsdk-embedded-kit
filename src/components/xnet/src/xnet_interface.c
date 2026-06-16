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

// @file xnet_interface.c
// @brief Implements network interface registration, state updates, and driver contracts.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xnet_core.h"
#include "xnet_defs.h"
#include "xnet_config.h"
#include "xnet_return.h"
#include "xnet_interface.h"
#include "xnet_ethernet.h"

#include "xnet_log.h"
#include "xassert.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xNET_Interface_Add(xNET_Context_t *net_ctx, xNET_Interface_Context_t *interface_ctx)
{
    xASSERT(net_ctx != NULL, "net_ctx is NULL");
    if (net_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xASSERT(net_ctx->is_initialized == true, "net_ctx not initialized");
    if (net_ctx->is_initialized == false)
    {
        return xRETURN_xERR_xNET_INVALID_STATE;
    }

    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if (interface_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xASSERT(interface_ctx->ops != NULL, "interface ops are NULL");
    if (interface_ctx->ops == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xASSERT(interface_ctx->ops->transmit != NULL, "transmit op is NULL");
    xASSERT(interface_ctx->ops->poll != NULL, "poll op is NULL");
    if ((interface_ctx->ops->transmit == NULL) || (interface_ctx->ops->poll == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (net_ctx->interface_count >= xNET_CONFIG_MAX_INTERFACES)
    {
        return xRETURN_xERR_xNET_BUFFER_TOO_SMALL;
    }

    // Set initial link/state configuration
    interface_ctx->net_ctx = net_ctx;
    interface_ctx->state = xNET_INTERFACE_STATE_DOWN;
    interface_ctx->next = NULL;

    // Initialize ARP cache
    xNET_ARP_Cache_Init(interface_ctx);

    // Append to net context linked list of interfaces
    if (net_ctx->interface_list == NULL)
    {
        net_ctx->interface_list = interface_ctx;
    }
    else
    {
        xNET_Interface_Context_t *curr = net_ctx->interface_list;
        while (curr->next != NULL)
        {
            curr = curr->next;
        }
        curr->next = interface_ctx;
    }

    net_ctx->interface_count++;

    return xRETURN_xNET_OK;
}

xRETURN_t xNET_Interface_Link_Set(xNET_Interface_Context_t *interface_ctx, bool is_link_up)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if (interface_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    interface_ctx->state = is_link_up ? xNET_INTERFACE_STATE_UP : xNET_INTERFACE_STATE_DOWN;

    return xRETURN_xNET_OK;
}

xRETURN_t xNET_Interface_RX_Frame(xNET_Interface_Context_t *interface_ctx, const uint8_t *frame, uint32_t length, uint32_t rx_flags)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if (interface_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xASSERT(frame != NULL, "frame is NULL");
    if (frame == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (length < xNET_ETHERNET_MIN_FRAME_SIZE)
    {
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    if (length > (xNET_ETHERNET_MTU + xNET_ETHERNET_HEADER_SIZE))
    {
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    xNET_Packet_Buffer_t *packet_buf = NULL;
    xRETURN_t ret = xNET_Packet_Alloc(interface_ctx->net_ctx, &packet_buf);
    if (ret != xRETURN_xNET_OK)
    {
        return ret;
    }

    if (length > packet_buf->capacity)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet_buf);
        return xRETURN_xERR_xNET_BUFFER_TOO_SMALL;
    }

    (void)memcpy(packet_buf->buffer, frame, length);
    packet_buf->data_offset = 0U;
    packet_buf->data_length = length;
    packet_buf->flags = rx_flags;

    ret = xNET_Ethernet_RX(interface_ctx, packet_buf);
    if (ret != xRETURN_xNET_OK)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet_buf);
    }

    return ret;
}

// EOF /////////////////////////////////////////////////////////////////////////////
