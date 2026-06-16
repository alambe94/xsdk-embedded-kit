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

// @file xnet_ipv4.c
// @brief Internet Protocol version 4 (IPv4) parser and output path implementation.
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
#include "xnet_arp.h"
#include "xnet_ipv4.h"
#include "xnet_icmp.h"
#include "xnet_udp.h"

#include "xnet_log.h"
#include "xassert.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////
static uint16_t g_ipv4_id = 0U;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t route_icmp(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet, const xNET_IPv4_Address_t *src_ip);
static xRETURN_t route_udp(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet, const xNET_IPv4_Address_t *src_ip);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t route_icmp(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet, const xNET_IPv4_Address_t *src_ip)
{
    return xNET_ICMP_RX(interface_ctx, packet, src_ip);
}

static xRETURN_t route_udp(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet, const xNET_IPv4_Address_t *src_ip)
{
    return xNET_UDP_RX(interface_ctx, packet, src_ip);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xNET_IPv4_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(packet != NULL, "packet is NULL");

    if ((interface_ctx == NULL) || (packet == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    uint8_t *data = xNET_Packet_Get_Data(packet);
    uint32_t length = xNET_Packet_Get_Length(packet);

    if (data == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (length < xNET_IPV4_HEADER_MIN_SIZE)
    {
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    uint8_t version = data[0] >> 4U;
    if (version != 4U)
    {
        return xRETURN_xERR_xNET_INVALID_PACKET;
    }

    uint8_t ihl = data[0] & 0x0FU;
    if (ihl < 5U)
    {
        return xRETURN_xERR_xNET_INVALID_PACKET;
    }

    uint32_t header_len = (uint32_t)ihl * 4U;
    if (length < header_len)
    {
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    uint16_t total_length = (uint16_t)(((uint16_t)data[2] << 8U) | data[3]);
    if ((total_length < header_len) || ((uint32_t)total_length > length))
    {
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    xNET_IPv4_Address_t src_ip;
    (void)memcpy(src_ip.addr, &data[12], 4);

    xNET_IPv4_Address_t dest_ip;
    (void)memcpy(dest_ip.addr, &data[16], 4);

    // Destination IP filtering
    bool is_for_us = (memcmp(dest_ip.addr, interface_ctx->ip_addr.addr, 4) == 0);

    if (!is_for_us)
    {
        // Limited Broadcast: 255.255.255.255
        bool is_limited_broadcast =
            (dest_ip.addr[0] == 0xFFU) && (dest_ip.addr[1] == 0xFFU) && (dest_ip.addr[2] == 0xFFU) && (dest_ip.addr[3] == 0xFFU);
        if (is_limited_broadcast)
        {
            is_for_us = true;
        }
    }

    if (!is_for_us)
    {
        // Multicast range: 224.0.0.0 to 239.255.255.255
        bool is_multicast = (dest_ip.addr[0] >= 224U) && (dest_ip.addr[0] <= 239U);
        if (is_multicast)
        {
            is_for_us = true;
        }
    }

    if (!is_for_us)
    {
        // Subnet directed broadcast: (dest_ip & ~netmask) == ~netmask
        bool is_subnet_broadcast = true;
        for (uint32_t i = 0U; i < 4U; i++)
        {
            uint8_t subnet_mask = interface_ctx->netmask.addr[i];
            uint8_t subnet_prefix = interface_ctx->ip_addr.addr[i] & subnet_mask;
            uint8_t dest_prefix = dest_ip.addr[i] & subnet_mask;
            uint8_t dest_host = dest_ip.addr[i] & ~subnet_mask;
            uint8_t expected_host = (uint8_t)~subnet_mask;
            if ((dest_prefix != subnet_prefix) || (dest_host != expected_host))
            {
                is_subnet_broadcast = false;
                break;
            }
        }
        if (is_subnet_broadcast)
        {
            is_for_us = true;
        }
    }

    if (!is_for_us)
    {
        return xRETURN_xERR_xNET_NOT_FOUND;
    }

    // Checksum verification
    bool checksum_valid = false;
    if ((interface_ctx->checksum_caps & xNET_CHECKSUM_CAP_IP_RX) != 0U)
    {
        if ((packet->flags & xNET_RX_FLAG_IP_CHECKSUM_INVALID) != 0U)
        {
            return xRETURN_xERR_xNET_CHECKSUM_FAILED;
        }
        else if ((packet->flags & xNET_RX_FLAG_IP_CHECKSUM_VALID) != 0U)
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
        uint16_t header_csum = xNET_Checksum_Calculate(data, header_len);
        if (header_csum != 0U)
        {
            return xRETURN_xERR_xNET_CHECKSUM_FAILED;
        }
    }

    // Reject Fragmented packets
    uint16_t flags_fragment = (uint16_t)(((uint16_t)data[6] << 8U) | data[7]);
    bool is_fragment = (flags_fragment & 0x3FFFU) != 0U;
    if (is_fragment)
    {
        return xRETURN_xERR_xNET_UNSUPPORTED;
    }

    // Reject IP options
    if (ihl > 5U)
    {
        return xRETURN_xERR_xNET_UNSUPPORTED;
    }

    // Strip/pull the IPv4 header
    xRETURN_t ret = xNET_Packet_Pull(packet, header_len);
    if (ret != xRETURN_xNET_OK)
    {
        return ret;
    }

    // Shrink data_length to strip any trailing Ethernet padding
    uint32_t payload_len = (uint32_t)total_length - header_len;
    if (packet->data_length > payload_len)
    {
        ret = xNET_Packet_Set_Length(packet, payload_len);
        if (ret != xRETURN_xNET_OK)
        {
            return ret;
        }
    }

    // Route based on protocol
    uint8_t protocol = data[9];
    if (protocol == xNET_IPV4_PROTOCOL_ICMP)
    {
        return route_icmp(interface_ctx, packet, &src_ip);
    }
    else if (protocol == xNET_IPV4_PROTOCOL_UDP)
    {
        return route_udp(interface_ctx, packet, &src_ip);
    }
    else
    {
        return xRETURN_xERR_xNET_UNSUPPORTED;
    }
}

xRETURN_t
xNET_IPv4_TX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet, const xNET_IPv4_Address_t *dest_ip, uint8_t protocol)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(packet != NULL, "packet is NULL");
    xASSERT(dest_ip != NULL, "dest_ip is NULL");

    if ((interface_ctx == NULL) || (packet == NULL) || (dest_ip == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xRETURN_t ret = xNET_Packet_Push(packet, xNET_IPV4_HEADER_MIN_SIZE);
    if (ret != xRETURN_xNET_OK)
    {
        return ret;
    }

    uint8_t *data = xNET_Packet_Get_Data(packet);
    if (data == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    // Version = 4, IHL = 5
    data[0] = 0x45U;
    // Type of Service = 0
    data[1] = 0x00U;
    // Total Length
    uint32_t total_len = xNET_Packet_Get_Length(packet);
    data[2] = (uint8_t)((total_len >> 8U) & 0xFFU);
    data[3] = (uint8_t)(total_len & 0xFFU);
    // Identification
    uint16_t id = g_ipv4_id;
    g_ipv4_id++;
    data[4] = (uint8_t)((id >> 8U) & 0xFFU);
    data[5] = (uint8_t)(id & 0xFFU);
    // Flags & Fragment Offset = 0
    data[6] = 0x00U;
    data[7] = 0x00U;
    // Time to Live
    data[8] = (uint8_t)xNET_CONFIG_IPV4_DEFAULT_TTL;
    // Protocol
    data[9] = protocol;
    // Checksum (initially 0)
    data[10] = 0x00U;
    data[11] = 0x00U;
    // Source IP
    (void)memcpy(&data[12], interface_ctx->ip_addr.addr, 4);
    // Destination IP
    (void)memcpy(&data[16], dest_ip->addr, 4);

    // Compute checksum
    if ((interface_ctx->checksum_caps & xNET_CHECKSUM_CAP_IP_TX) != 0U)
    {
        packet->flags |= xNET_TX_FLAG_CSUM_IP;
    }
    else
    {
        uint16_t checksum = xNET_Checksum_Calculate(data, xNET_IPV4_HEADER_MIN_SIZE);
        data[10] = (uint8_t)((checksum >> 8U) & 0xFFU);
        data[11] = (uint8_t)(checksum & 0xFFU);
    }

    const xNET_IPv4_Address_t *next_hop = dest_ip;
    bool is_multicast = (dest_ip->addr[0] >= 224U) && (dest_ip->addr[0] <= 239U);
    bool is_broadcast =
        (dest_ip->addr[0] == 0xFFU) && (dest_ip->addr[1] == 0xFFU) && (dest_ip->addr[2] == 0xFFU) && (dest_ip->addr[3] == 0xFFU);
    bool on_subnet = true;

    if (!is_multicast && !is_broadcast)
    {
        for (uint32_t i = 0U; i < 4U; i++)
        {
            if ((dest_ip->addr[i] & interface_ctx->netmask.addr[i]) != (interface_ctx->ip_addr.addr[i] & interface_ctx->netmask.addr[i]))
            {
                on_subnet = false;
                break;
            }
        }
    }

    if (!on_subnet)
    {
        bool gateway_non_zero = (interface_ctx->gateway.addr[0] != 0U) || (interface_ctx->gateway.addr[1] != 0U) ||
                                (interface_ctx->gateway.addr[2] != 0U) || (interface_ctx->gateway.addr[3] != 0U);
        if (gateway_non_zero)
        {
            next_hop = &interface_ctx->gateway;
        }
    }

    return xNET_ARP_Resolve(interface_ctx, next_hop, packet);
}

xRETURN_t xNET_IPv4_Config_Static(xNET_Interface_Context_t *interface_ctx,
                                  const xNET_IPv4_Address_t *ip_addr,
                                  const xNET_IPv4_Address_t *netmask,
                                  const xNET_IPv4_Address_t *gateway)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(ip_addr != NULL, "ip_addr is NULL");
    xASSERT(netmask != NULL, "netmask is NULL");

    if ((interface_ctx == NULL) || (ip_addr == NULL) || (netmask == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    // Local IP Address validation:
    // Reject loopback (127.0.0.0/8)
    if (ip_addr->addr[0] == 127U)
    {
        return xRETURN_xERR_xNET_INVALID_ARGUMENT;
    }
    // Reject multicast (224.0.0.0/4)
    if ((ip_addr->addr[0] >= 224U) && (ip_addr->addr[0] <= 239U))
    {
        return xRETURN_xERR_xNET_INVALID_ARGUMENT;
    }
    // Reject Class E / Reserved (240.0.0.0/4)
    if (ip_addr->addr[0] >= 240U)
    {
        return xRETURN_xERR_xNET_INVALID_ARGUMENT;
    }
    // Reject broadcast (255.255.255.255)
    if ((ip_addr->addr[0] == 255U) && (ip_addr->addr[1] == 255U) && (ip_addr->addr[2] == 255U) && (ip_addr->addr[3] == 255U))
    {
        return xRETURN_xERR_xNET_INVALID_ARGUMENT;
    }
    // Reject unspecified (0.0.0.0)
    if ((ip_addr->addr[0] == 0U) && (ip_addr->addr[1] == 0U) && (ip_addr->addr[2] == 0U) && (ip_addr->addr[3] == 0U))
    {
        return xRETURN_xERR_xNET_INVALID_ARGUMENT;
    }

    // Subnet Mask validation:
    // Reject all-zero netmask
    uint32_t mask_val = ((uint32_t)netmask->addr[0] << 24U) | ((uint32_t)netmask->addr[1] << 16U) | ((uint32_t)netmask->addr[2] << 8U) |
                        (uint32_t)netmask->addr[3];
    if (mask_val == 0U)
    {
        return xRETURN_xERR_xNET_INVALID_ARGUMENT;
    }
    // Enforce contiguous bits
    uint32_t inv_mask = ~mask_val;
    uint32_t test = inv_mask + 1U;
    if ((test & (test - 1U)) != 0U)
    {
        return xRETURN_xERR_xNET_INVALID_ARGUMENT;
    }

    // Default Gateway validation:
    // If gateway is provided, it must reside in the same subnet
    if (gateway != NULL)
    {
        bool gateway_non_zero =
            (gateway->addr[0] != 0U) || (gateway->addr[1] != 0U) || (gateway->addr[2] != 0U) || (gateway->addr[3] != 0U);
        if (gateway_non_zero)
        {
            for (uint32_t i = 0U; i < 4U; i++)
            {
                if ((gateway->addr[i] & netmask->addr[i]) != (ip_addr->addr[i] & netmask->addr[i]))
                {
                    return xRETURN_xERR_xNET_INVALID_ARGUMENT;
                }
            }
        }
    }

    // Apply configuration:
    interface_ctx->ip_addr = *ip_addr;
    interface_ctx->netmask = *netmask;
    if (gateway != NULL)
    {
        interface_ctx->gateway = *gateway;
    }
    else
    {
        (void)memset(interface_ctx->gateway.addr, 0, 4);
    }

    // DNS server fields initialization (zero by default)
    (void)memset(interface_ctx->dns_primary.addr, 0, 4);
    (void)memset(interface_ctx->dns_secondary.addr, 0, 4);

    // Flush ARP Cache:
    // Re-initialize ARP entries to FREE when configuration changes
    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        interface_ctx->arp_cache[i].state = xNET_ARP_ENTRY_FREE;
        interface_ctx->arp_cache[i].timeout_ms = 0U;
        interface_ctx->arp_cache[i].retry_ms = 0U;
        interface_ctx->arp_cache[i].retries_left = 0U;
        (void)memset(interface_ctx->arp_cache[i].ip_addr.addr, 0, 4);
        (void)memset(interface_ctx->arp_cache[i].mac_addr.addr, 0, 6);
        // Release any pending packets
        if (interface_ctx->arp_cache[i].pending_packet != NULL)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, interface_ctx->arp_cache[i].pending_packet);
            interface_ctx->arp_cache[i].pending_packet = NULL;
        }
    }

    return xRETURN_xNET_OK;
}

// EOF /////////////////////////////////////////////////////////////////////////////
