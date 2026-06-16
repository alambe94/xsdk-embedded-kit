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

// @file xnet_ethernet.c
// @brief Ethernet II parser, builder, and RX/TX packet processing implementation.
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
#include "xnet_ethernet.h"
#include "xnet_arp.h"
#include "xnet_ipv4.h"

#include "xnet_log.h"
#include "xassert.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

bool xNET_Ethernet_Is_Broadcast(const xNET_MAC_Address_t *mac)
{
    xASSERT(mac != NULL, "mac is NULL");
    if (mac == NULL)
    {
        return false;
    }

    return (mac->addr[0] == 0xFFU) && (mac->addr[1] == 0xFFU) && (mac->addr[2] == 0xFFU) && (mac->addr[3] == 0xFFU) &&
           (mac->addr[4] == 0xFFU) && (mac->addr[5] == 0xFFU);
}

bool xNET_Ethernet_Is_Multicast(const xNET_MAC_Address_t *mac)
{
    xASSERT(mac != NULL, "mac is NULL");
    if (mac == NULL)
    {
        return false;
    }

    return (mac->addr[0] & 0x01U) != 0U;
}

xRETURN_t xNET_Ethernet_Parse(const uint8_t *data, uint32_t length, xNET_MAC_Address_t *dest, xNET_MAC_Address_t *src, uint16_t *ethertype)
{
    xASSERT(data != NULL, "data is NULL");
    xASSERT(dest != NULL, "dest is NULL");
    xASSERT(src != NULL, "src is NULL");
    xASSERT(ethertype != NULL, "ethertype is NULL");

    if ((data == NULL) || (dest == NULL) || (src == NULL) || (ethertype == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (length < xNET_ETHERNET_HEADER_SIZE)
    {
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    (void)memcpy(dest->addr, &data[0], xNET_MAC_ADDRESS_SIZE);
    (void)memcpy(src->addr, &data[6], xNET_MAC_ADDRESS_SIZE);

    uint16_t type_raw = ((uint16_t)data[12] << 8) | (uint16_t)data[13];
    *ethertype = type_raw;

    return xRETURN_xNET_OK;
}

xRETURN_t
xNET_Ethernet_Build(uint8_t *data, uint32_t length, const xNET_MAC_Address_t *dest, const xNET_MAC_Address_t *src, uint16_t ethertype)
{
    xASSERT(data != NULL, "data is NULL");
    xASSERT(dest != NULL, "dest is NULL");
    xASSERT(src != NULL, "src is NULL");

    if ((data == NULL) || (dest == NULL) || (src == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (length < xNET_ETHERNET_HEADER_SIZE)
    {
        return xRETURN_xERR_xNET_BUFFER_TOO_SMALL;
    }

    (void)memcpy(&data[0], dest->addr, xNET_MAC_ADDRESS_SIZE);
    (void)memcpy(&data[6], src->addr, xNET_MAC_ADDRESS_SIZE);

    data[12] = (uint8_t)((ethertype >> 8) & 0xFFU);
    data[13] = (uint8_t)(ethertype & 0xFFU);

    return xRETURN_xNET_OK;
}

xRETURN_t xNET_Ethernet_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet)
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

    if (length < xNET_ETHERNET_HEADER_SIZE)
    {
        return xRETURN_xERR_xNET_INVALID_LENGTH;
    }

    xNET_MAC_Address_t dest = {0};
    xNET_MAC_Address_t src = {0};
    uint16_t ethertype = 0U;

    xRETURN_t ret = xNET_Ethernet_Parse(data, length, &dest, &src, &ethertype);
    if (ret != xRETURN_xNET_OK)
    {
        return ret;
    }

    // Destination MAC filtering:
    // Accept only:
    // 1. Unicast matching local interface MAC
    // 2. Broadcast
    // 3. Multicast
    bool is_for_us = false;
    if ((memcmp(dest.addr, interface_ctx->mac_addr.addr, xNET_MAC_ADDRESS_SIZE) == 0) || xNET_Ethernet_Is_Broadcast(&dest) ||
        xNET_Ethernet_Is_Multicast(&dest))
    {
        is_for_us = true;
    }
    else
    {
        return xRETURN_xERR_xNET_NOT_FOUND;
    }

    (void)is_for_us;

    // Pull/consume the Ethernet header
    ret = xNET_Packet_Pull(packet, xNET_ETHERNET_HEADER_SIZE);
    if (ret != xRETURN_xNET_OK)
    {
        return ret;
    }

    // Route packet based on ethertype
    if (ethertype == xNET_ETHERTYPE_ARP)
    {
        return xNET_ARP_RX(interface_ctx, packet);
    }
    else if (ethertype == xNET_ETHERTYPE_IPV4)
    {
        return xNET_IPv4_RX(interface_ctx, packet);
    }
    else
    {
        return xRETURN_xERR_xNET_UNSUPPORTED;
    }
}

xRETURN_t xNET_Ethernet_TX(xNET_Interface_Context_t *interface_ctx,
                           xNET_Packet_Buffer_t *packet,
                           const xNET_MAC_Address_t *dest_mac,
                           uint16_t ethertype)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(packet != NULL, "packet is NULL");
    xASSERT(dest_mac != NULL, "dest_mac is NULL");

    if ((interface_ctx == NULL) || (packet == NULL) || (dest_mac == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (interface_ctx->state != xNET_INTERFACE_STATE_UP)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_LINK_DOWN;
    }

    if ((interface_ctx->ops == NULL) || (interface_ctx->ops->transmit == NULL))
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    // Validate minimum payload size (46 bytes). If smaller, we pad it with zeros.
    uint32_t current_len = xNET_Packet_Get_Length(packet);
    if (current_len < 46U)
    {
        uint32_t pad_len = 46U - current_len;
        uint8_t *data = xNET_Packet_Get_Data(packet);
        if (data == NULL)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
            return xRETURN_xERR_xNET_NULL_POINTER;
        }

        // Check if there is enough capacity for padding
        if (packet->data_offset + 46U > packet->capacity)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
            return xRETURN_xERR_xNET_BUFFER_TOO_SMALL;
        }

        (void)memset(data + current_len, 0, pad_len);
        xRETURN_t pad_ret = xNET_Packet_Set_Length(packet, 46U);
        if (pad_ret != xRETURN_xNET_OK)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
            return pad_ret;
        }
    }

    // Push the Ethernet header space (14 bytes)
    xRETURN_t ret = xNET_Packet_Push(packet, xNET_ETHERNET_HEADER_SIZE);
    if (ret != xRETURN_xNET_OK)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return ret;
    }

    uint8_t *header_data = xNET_Packet_Get_Data(packet);
    uint32_t header_len = xNET_Packet_Get_Length(packet);
    if (header_data == NULL)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    ret = xNET_Ethernet_Build(header_data, header_len, dest_mac, &interface_ctx->mac_addr, ethertype);
    if (ret != xRETURN_xNET_OK)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return ret;
    }

    // Call interface driver transmit callback
    xRETURN_t tx_ret = interface_ctx->ops->transmit(interface_ctx->driver_ctx, xNET_Packet_Get_Data(packet), xNET_Packet_Get_Length(packet),
                                                    packet->flags);

    // Release packet buffer (ownership is consumed by transmitting)
    (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);

    return tx_ret;
}

// EOF /////////////////////////////////////////////////////////////////////////////
