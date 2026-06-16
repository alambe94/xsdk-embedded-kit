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

// @file xnet_core.c
// @brief Core implementation for the xNET module initialization, loop, and helpers.
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
#include "xnet_arp.h"

#include "xnet_log.h"
#include "xassert.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xNET_Init(xNET_Context_t *net_ctx, const xNET_Config_t *net_config)
{
    xASSERT(net_ctx != NULL, "net_ctx is NULL");
    if (net_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xASSERT(net_config != NULL, "net_config is NULL");
    if (net_config == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xASSERT(net_config->packet_pool_buffer != NULL, "packet_pool_buffer is NULL");
    if (net_config->packet_pool_buffer == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    uintptr_t start_addr = (uintptr_t)net_config->packet_pool_buffer;
    uintptr_t aligned_start = (start_addr + (xNET_PACKET_BUFFER_ALIGNMENT - 1U)) & ~(uintptr_t)(xNET_PACKET_BUFFER_ALIGNMENT - 1U);
    uint32_t alignment_loss = (uint32_t)(aligned_start - start_addr);

    if (net_config->packet_pool_buffer_size < (alignment_loss + (xNET_CONFIG_PACKET_POOL_SIZE * xNET_PACKET_FRAME_SIZE)))
    {
        return xRETURN_xERR_xNET_BUFFER_TOO_SMALL;
    }

    (void)memset(net_ctx, 0, sizeof(xNET_Context_t));

    net_ctx->config.packet_pool_buffer = net_config->packet_pool_buffer;
    net_ctx->config.packet_pool_buffer_size = net_config->packet_pool_buffer_size;

    uint8_t *curr_buf = (uint8_t *)aligned_start;
    for (uint32_t i = 0U; i < xNET_CONFIG_PACKET_POOL_SIZE; i++)
    {
        net_ctx->packet_pool[i].buffer = curr_buf;
        net_ctx->packet_pool[i].capacity = xNET_PACKET_FRAME_SIZE;
        net_ctx->packet_pool[i].data_offset = 0U;
        net_ctx->packet_pool[i].data_length = 0U;
        net_ctx->packet_pool[i].is_in_use = false;
        net_ctx->packet_pool[i].flags = 0U;

        curr_buf += xNET_PACKET_FRAME_SIZE;
    }

    net_ctx->system_ticks = 0U;
    net_ctx->is_initialized = true;

    return xRETURN_xNET_OK;
}

xRETURN_t xNET_Process(xNET_Context_t *net_ctx)
{
    xASSERT(net_ctx != NULL, "net_ctx is NULL");
    if (net_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xASSERT(net_ctx->is_initialized == true, "net_ctx is not initialized");
    if (net_ctx->is_initialized == false)
    {
        return xRETURN_xERR_xNET_INVALID_STATE;
    }

    xNET_Interface_Context_t *curr = net_ctx->interface_list;
    for (uint32_t i = 0U; (curr != NULL) && (i < xNET_CONFIG_MAX_INTERFACES); i++)
    {
        if ((curr->ops != NULL) && (curr->ops->poll != NULL))
        {
            xRETURN_t status = curr->ops->poll(curr->driver_ctx);
            if (status != xRETURN_xNET_OK)
            {
                return status;
            }
        }

        // Process deferred ARP retransmissions
        for (uint32_t j = 0U; j < xNET_CONFIG_ARP_CACHE_SIZE; j++)
        {
            xNET_ARP_Entry_t *entry = &curr->arp_cache[j];
            if ((entry->state == xNET_ARP_ENTRY_PENDING) && entry->needs_retransmit)
            {
                entry->needs_retransmit = false;
                (void)xNET_ARP_Send(curr, xNET_ARP_OP_REQUEST, NULL, &entry->ip_addr);
            }
        }

        curr = curr->next;
    }

    return xRETURN_xNET_OK;
}

uint16_t xNET_Checksum_Calculate(const void *data, uint32_t length)
{
    xASSERT(data != NULL, "data is NULL");
    if (data == NULL)
    {
        return 0U;
    }

    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t sum = 0U;
    uint32_t len = length;

    while (len > 1U)
    {
        uint16_t word = (uint16_t)(((uint16_t)ptr[0] << 8U) | (uint16_t)ptr[1]);
        sum += (uint32_t)word;
        ptr += 2U;
        len -= 2U;
    }

    if (len > 0U)
    {
        uint16_t word = (uint16_t)((uint16_t)ptr[0] << 8U);
        sum += (uint32_t)word;
    }

    while ((sum >> 16U) != 0U)
    {
        sum = (sum & 0xFFFFU) + (sum >> 16U);
    }

    return (uint16_t)(~((uint16_t)sum));
}

uint16_t xNET_Checksum_Calculate_Pseudo(const void *data,
                                        uint32_t length,
                                        const xNET_IPv4_Address_t *src,
                                        const xNET_IPv4_Address_t *dest,
                                        uint8_t protocol,
                                        uint16_t proto_len)
{
    xASSERT(src != NULL, "src is NULL");
    xASSERT(dest != NULL, "dest is NULL");
    if ((src == NULL) || (dest == NULL))
    {
        return 0U;
    }

    uint32_t sum = 0U;

    // Add source IP (2 x 16-bit words)
    sum += (uint32_t)(((uint16_t)src->addr[0] << 8U) | (uint16_t)src->addr[1]);
    sum += (uint32_t)(((uint16_t)src->addr[2] << 8U) | (uint16_t)src->addr[3]);

    // Add dest IP (2 x 16-bit words)
    sum += (uint32_t)(((uint16_t)dest->addr[0] << 8U) | (uint16_t)dest->addr[1]);
    sum += (uint32_t)(((uint16_t)dest->addr[2] << 8U) | (uint16_t)dest->addr[3]);

    // Add protocol and proto length
    sum += (uint32_t)protocol;
    sum += (uint32_t)proto_len;

    // Add payload checksum (without inverting it first)
    if ((data != NULL) && (length > 0U))
    {
        const uint8_t *ptr = (const uint8_t *)data;
        uint32_t len = length;

        while (len > 1U)
        {
            uint16_t word = (uint16_t)(((uint16_t)ptr[0] << 8U) | (uint16_t)ptr[1]);
            sum += (uint32_t)word;
            ptr += 2U;
            len -= 2U;
        }

        if (len > 0U)
        {
            uint16_t word = (uint16_t)((uint16_t)ptr[0] << 8U);
            sum += (uint32_t)word;
        }
    }

    while ((sum >> 16U) != 0U)
    {
        sum = (sum & 0xFFFFU) + (sum >> 16U);
    }

    return (uint16_t)(~((uint16_t)sum));
}

// EOF /////////////////////////////////////////////////////////////////////////////
