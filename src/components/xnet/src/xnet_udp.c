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

// @file xnet_udp.c
// @brief User Datagram Protocol (UDP) implementation.
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
#include "xnet_udp.h"

#include "xnet_log.h"
#include "xassert.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xNET_UDP_Open(xNET_Context_t *net_ctx, xNET_UDP_Context_t *udp_ctx, uint16_t local_port, xNET_UDP_Receive_Callback_t on_received)
{
    xASSERT(net_ctx != NULL, "net_ctx is NULL");
    xASSERT(udp_ctx != NULL, "udp_ctx is NULL");
    xASSERT(on_received != NULL, "on_received is NULL");

    if ((net_ctx == NULL) || (udp_ctx == NULL) || (on_received == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    uint16_t port = local_port;
    if (port == 0U)
    {
        // Allocate ephemeral port in range [49152, 65535]
        static uint16_t next_ephemeral = 49152U;
        uint16_t start_port = next_ephemeral;
        bool found = false;

        do
        {
            uint16_t test_port = next_ephemeral;
            next_ephemeral++;
            if (next_ephemeral < 49152U)
            {
                next_ephemeral = 49152U;
            }

            // Check if test_port is already in use
            bool in_use = false;
            const xNET_UDP_Context_t *curr = net_ctx->udp_list;
            while (curr != NULL)
            {
                if (curr->local_port == test_port)
                {
                    in_use = true;
                    break;
                }
                curr = curr->next;
            }

            if (!in_use)
            {
                port = test_port;
                found = true;
                break;
            }
        } while (next_ephemeral != start_port);

        if (!found)
        {
            return xRETURN_xERR_xNET_NO_PACKET_BUFFER;
        }
    }
    else
    {
        // Verify local_port is not already in use
        const xNET_UDP_Context_t *curr = net_ctx->udp_list;
        while (curr != NULL)
        {
            if (curr->local_port == port)
            {
                return xRETURN_xERR_xNET_INVALID_ARGUMENT;
            }
            curr = curr->next;
        }
    }

    udp_ctx->net_ctx = net_ctx;
    udp_ctx->local_port = port;
    udp_ctx->on_received = on_received;
    udp_ctx->next = net_ctx->udp_list;
    net_ctx->udp_list = udp_ctx;

    return xRETURN_xNET_OK;
}

xRETURN_t xNET_UDP_Close(xNET_UDP_Context_t *udp_ctx)
{
    xASSERT(udp_ctx != NULL, "udp_ctx is NULL");

    if (udp_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xNET_Context_t *net_ctx = udp_ctx->net_ctx;
    if (net_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NOT_FOUND;
    }

    xNET_UDP_Context_t *curr = net_ctx->udp_list;
    xNET_UDP_Context_t *prev = NULL;
    bool found = false;

    while (curr != NULL)
    {
        if (curr == udp_ctx)
        {
            if (prev == NULL)
            {
                net_ctx->udp_list = curr->next;
            }
            else
            {
                prev->next = curr->next;
            }
            found = true;
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    if (!found)
    {
        return xRETURN_xERR_xNET_NOT_FOUND;
    }

    udp_ctx->net_ctx = NULL;
    udp_ctx->local_port = 0U;
    udp_ctx->on_received = NULL;
    udp_ctx->next = NULL;

    return xRETURN_xNET_OK;
}

xRETURN_t xNET_UDP_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet, const xNET_IPv4_Address_t *src_ip)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(packet != NULL, "packet is NULL");
    xASSERT(src_ip != NULL, "src_ip is NULL");

    if ((interface_ctx == NULL) || (packet == NULL) || (src_ip == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    uint32_t length = xNET_Packet_Get_Length(packet);
    if (length < xNET_UDP_HEADER_SIZE)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    uint8_t *data = xNET_Packet_Get_Data(packet);
    if (data == NULL)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    uint16_t src_port = (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
    uint16_t dest_port = (uint16_t)(((uint16_t)data[2] << 8U) | data[3]);
    uint16_t udp_len = (uint16_t)(((uint16_t)data[4] << 8U) | data[5]);
    uint16_t udp_csum = (uint16_t)(((uint16_t)data[6] << 8U) | data[7]);

    if ((udp_len < xNET_UDP_HEADER_SIZE) || ((uint32_t)udp_len > length))
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    // Trim trailing padding if actual packet is larger than UDP length
    if (length > (uint32_t)udp_len)
    {
        xRETURN_t trim_ret = xNET_Packet_Set_Length(packet, (uint32_t)udp_len);
        if (trim_ret != xRETURN_xNET_OK)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
            return trim_ret;
        }
        length = (uint32_t)udp_len;
    }

    // Checksum validation
    if (udp_csum != 0U)
    {
        bool checksum_valid = false;
        if ((interface_ctx->checksum_caps & xNET_CHECKSUM_CAP_UDP_RX) != 0U)
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
        }

        if (!checksum_valid)
        {
            uint16_t calc_csum =
                xNET_Checksum_Calculate_Pseudo(data, length, src_ip, &interface_ctx->ip_addr, xNET_IPV4_PROTOCOL_UDP, (uint16_t)length);
            if (calc_csum != 0U)
            {
                (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
                return xRETURN_xERR_xNET_CHECKSUM_FAILED;
            }
        }
    }

    xNET_Context_t *net_ctx = interface_ctx->net_ctx;
    xASSERT(net_ctx != NULL, "net_ctx is NULL");

    if (net_ctx == NULL)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    // Search bound contexts
    xNET_UDP_Context_t *udp_ctx = net_ctx->udp_list;
    xNET_UDP_Context_t *match = NULL;
    while (udp_ctx != NULL)
    {
        if (udp_ctx->local_port == dest_port)
        {
            match = udp_ctx;
            break;
        }
        udp_ctx = udp_ctx->next;
    }

    if (match == NULL)
    {
        (void)xNET_Packet_Release(net_ctx, packet);
        return xRETURN_xERR_xNET_NOT_FOUND;
    }

    xRETURN_t pull_ret = xNET_Packet_Pull(packet, xNET_UDP_HEADER_SIZE);
    if (pull_ret != xRETURN_xNET_OK)
    {
        (void)xNET_Packet_Release(net_ctx, packet);
        return pull_ret;
    }

    xRETURN_t cb_ret = match->on_received(match, src_ip, src_port, xNET_Packet_Get_Data(packet), xNET_Packet_Get_Length(packet));

    (void)xNET_Packet_Release(net_ctx, packet);

    return cb_ret;
}

xRETURN_t xNET_UDP_Send_To(xNET_UDP_Context_t *udp_ctx,
                           const xNET_IPv4_Address_t *remote_addr,
                           uint16_t remote_port,
                           const uint8_t *data,
                           uint32_t length)
{
    xASSERT(udp_ctx != NULL, "udp_ctx is NULL");
    xASSERT(remote_addr != NULL, "remote_addr is NULL");
    xASSERT(data != NULL, "data is NULL");

    if ((udp_ctx == NULL) || (remote_addr == NULL) || (data == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xNET_Context_t *net_ctx = udp_ctx->net_ctx;
    xASSERT(net_ctx != NULL, "net_ctx is NULL");

    if (net_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    // Route target interface (multihoming lookup)
    xNET_Interface_Context_t *interface_ctx = net_ctx->interface_list;
    xNET_Interface_Context_t *target_interface = NULL;
    while (interface_ctx != NULL)
    {
        bool match = true;
        for (uint32_t i = 0U; i < 4U; i++)
        {
            if ((remote_addr->addr[i] & interface_ctx->netmask.addr[i]) !=
                (interface_ctx->ip_addr.addr[i] & interface_ctx->netmask.addr[i]))
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            target_interface = interface_ctx;
            break;
        }
        interface_ctx = interface_ctx->next;
    }

    if (target_interface == NULL)
    {
        target_interface = net_ctx->interface_list;
    }

    if (target_interface == NULL)
    {
        return xRETURN_xERR_xNET_LINK_DOWN;
    }

    xNET_Packet_Buffer_t *packet = NULL;
    xRETURN_t alloc_ret = xNET_Packet_Alloc(net_ctx, &packet);
    if (alloc_ret != xRETURN_xNET_OK)
    {
        return alloc_ret;
    }

    packet->data_offset = 64U;
    packet->data_length = length;
    uint8_t *pkt_data = xNET_Packet_Get_Data(packet);
    if (pkt_data == NULL)
    {
        (void)xNET_Packet_Release(net_ctx, packet);
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    (void)memcpy(pkt_data, data, length);

    xRETURN_t push_ret = xNET_Packet_Push(packet, xNET_UDP_HEADER_SIZE);
    if (push_ret != xRETURN_xNET_OK)
    {
        (void)xNET_Packet_Release(net_ctx, packet);
        return push_ret;
    }

    uint8_t *udp_hdr = xNET_Packet_Get_Data(packet);
    if (udp_hdr == NULL)
    {
        (void)xNET_Packet_Release(net_ctx, packet);
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    uint16_t udp_tot_len = (uint16_t)(length + xNET_UDP_HEADER_SIZE);

    udp_hdr[0] = (uint8_t)((udp_ctx->local_port >> 8U) & 0xFFU);
    udp_hdr[1] = (uint8_t)(udp_ctx->local_port & 0xFFU);
    udp_hdr[2] = (uint8_t)((remote_port >> 8U) & 0xFFU);
    udp_hdr[3] = (uint8_t)(remote_port & 0xFFU);
    udp_hdr[4] = (uint8_t)((udp_tot_len >> 8U) & 0xFFU);
    udp_hdr[5] = (uint8_t)(udp_tot_len & 0xFFU);
    udp_hdr[6] = 0x00U;
    udp_hdr[7] = 0x00U;

    // Checksum calculation
    if ((target_interface->checksum_caps & xNET_CHECKSUM_CAP_UDP_TX) != 0U)
    {
        packet->flags |= xNET_TX_FLAG_CSUM_UDP;
    }
    else
    {
        uint16_t checksum = xNET_Checksum_Calculate_Pseudo(udp_hdr, udp_tot_len, &target_interface->ip_addr, remote_addr,
                                                           xNET_IPV4_PROTOCOL_UDP, udp_tot_len);
        if (checksum == 0U)
        {
            checksum = 0xFFFFU;
        }
        udp_hdr[6] = (uint8_t)((checksum >> 8U) & 0xFFU);
        udp_hdr[7] = (uint8_t)(checksum & 0xFFU);
    }

    return xNET_IPv4_TX(target_interface, packet, remote_addr, xNET_IPV4_PROTOCOL_UDP);
}

// EOF /////////////////////////////////////////////////////////////////////////////
