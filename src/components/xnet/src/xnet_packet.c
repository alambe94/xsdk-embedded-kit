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

// @file xnet_packet.c
// @brief Packet allocation and pool release mechanisms for the xNET module.
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
#include "xnet_packet.h"

#include "xnet_log.h"
#include "xassert.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xNET_Packet_Alloc(xNET_Context_t *net_ctx, xNET_Packet_Buffer_t **packet_buf)
{
    xASSERT(net_ctx != NULL, "net_ctx is NULL");
    xASSERT(packet_buf != NULL, "packet_buf is NULL");
    if ((net_ctx == NULL) || (packet_buf == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (net_ctx->is_initialized == false)
    {
        return xRETURN_xERR_xNET_INVALID_STATE;
    }

    for (uint32_t i = 0U; i < xNET_CONFIG_PACKET_POOL_SIZE; i++)
    {
        if (net_ctx->packet_pool[i].is_in_use == false)
        {
            net_ctx->packet_pool[i].is_in_use = true;
            net_ctx->packet_pool[i].data_offset = 0U;
            net_ctx->packet_pool[i].data_length = 0U;
            net_ctx->packet_pool[i].flags = 0U;

            *packet_buf = &net_ctx->packet_pool[i];
            return xRETURN_xNET_OK;
        }
    }

    return xRETURN_xERR_xNET_NO_PACKET_BUFFER;
}

xRETURN_t xNET_Packet_Release(xNET_Context_t *net_ctx, xNET_Packet_Buffer_t *packet_buf)
{
    xASSERT(net_ctx != NULL, "net_ctx is NULL");
    xASSERT(packet_buf != NULL, "packet_buf is NULL");
    if ((net_ctx == NULL) || (packet_buf == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (net_ctx->is_initialized == false)
    {
        return xRETURN_xERR_xNET_INVALID_STATE;
    }

    bool is_valid_ptr = false;
    for (uint32_t i = 0U; i < xNET_CONFIG_PACKET_POOL_SIZE; i++)
    {
        if (&net_ctx->packet_pool[i] == packet_buf)
        {
            is_valid_ptr = true;
            break;
        }
    }

    if (is_valid_ptr == false)
    {
        return xRETURN_xERR_xNET_INVALID_ARGUMENT;
    }

    if (packet_buf->is_in_use == false)
    {
        return xRETURN_xERR_xNET_INVALID_STATE;
    }

    packet_buf->is_in_use = false;
    return xRETURN_xNET_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
