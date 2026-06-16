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

// @file xnet_icmp.c
// @brief Internet Control Message Protocol (ICMP) parser and Echo handler implementation.
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
#include "xnet_packet.h"
#include "xnet_ipv4.h"
#include "xnet_icmp.h"

#include "xnet_log.h"
#include "xassert.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xNET_ICMP_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet, const xNET_IPv4_Address_t *src_ip)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(packet != NULL, "packet is NULL");
    xASSERT(src_ip != NULL, "src_ip is NULL");

    if ((interface_ctx == NULL) || (packet == NULL) || (src_ip == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    uint8_t *data = xNET_Packet_Get_Data(packet);
    uint32_t length = xNET_Packet_Get_Length(packet);

    if (data == NULL)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (length < xNET_ICMP_HEADER_SIZE)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    // Checksum verification
    bool checksum_valid = false;
    if ((interface_ctx->checksum_caps & xNET_CHECKSUM_CAP_ICMP_RX) != 0U)
    {
        if ((packet->flags & xNET_RX_FLAG_L4_CHECKSUM_INVALID) != 0U)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
            return xRETURN_xERR_xNET_CHECKSUM_FAILED;
        }
        else if ((packet->flags & xNET_RX_FLAG_L4_CHECKSUM_VALID) != 0U)
        {
            checksum_valid = true;
        }
        else
        {
            // Fallback to software check
        }
    }

    if (!checksum_valid)
    {
        uint16_t icmp_csum = xNET_Checksum_Calculate(data, length);
        if (icmp_csum != 0U)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
            return xRETURN_xERR_xNET_CHECKSUM_FAILED;
        }
    }

    uint8_t type = data[0];
    uint8_t code = data[1];

    if ((type == xNET_ICMP_TYPE_ECHO_REQUEST) && (code == 0U))
    {
        // Process Echo Request - respond with Echo Reply
        data[0] = xNET_ICMP_TYPE_ECHO_REPLY;

        // Compute checksum
        if ((interface_ctx->checksum_caps & xNET_CHECKSUM_CAP_ICMP_TX) != 0U)
        {
            packet->flags |= xNET_TX_FLAG_CSUM_ICMP;
            data[2] = 0U;
            data[3] = 0U;
        }
        else
        {
            // Software checksum: set fields to 0 first
            data[2] = 0U;
            data[3] = 0U;
            uint16_t checksum = xNET_Checksum_Calculate(data, length);
            data[2] = (uint8_t)((checksum >> 8U) & 0xFFU);
            data[3] = (uint8_t)(checksum & 0xFFU);
        }

        return xNET_IPv4_TX(interface_ctx, packet, src_ip, xNET_IPV4_PROTOCOL_ICMP);
    }
    else
    {
        // Unsupported ICMP type/code
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_UNSUPPORTED;
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
