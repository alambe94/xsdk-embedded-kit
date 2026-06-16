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

// @file xnet_arp.c
// @brief Address Resolution Protocol (ARP) cache and packet processing implementation.
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
#include "xnet_timer.h"

#include "xnet_log.h"
#include "xassert.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xNET_ARP_Entry_t *arp_cache_allocate_entry(xNET_Interface_Context_t *interface_ctx);
static void
arp_cache_update(xNET_Interface_Context_t *interface_ctx, const xNET_IPv4_Address_t *ip_addr, const xNET_MAC_Address_t *mac_addr);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xNET_ARP_Entry_t *arp_cache_allocate_entry(xNET_Interface_Context_t *interface_ctx)
{
    // 1. Look for free entries
    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        if (interface_ctx->arp_cache[i].state == xNET_ARP_ENTRY_FREE)
        {
            interface_ctx->arp_cache[i].needs_retransmit = false;
            return &interface_ctx->arp_cache[i];
        }
    }

    uint32_t current_time = interface_ctx->net_ctx->system_ticks;

    // 2. Look for the oldest resolved entry (minimum remaining time relative to current_time)
    xNET_ARP_Entry_t *oldest_resolved = NULL;
    int32_t min_timeout = 0x7FFFFFFF;
    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        if (interface_ctx->arp_cache[i].state == xNET_ARP_ENTRY_RESOLVED)
        {
            int32_t remaining = (int32_t)(interface_ctx->arp_cache[i].timeout_ms - current_time);
            if (remaining < min_timeout)
            {
                min_timeout = remaining;
                oldest_resolved = &interface_ctx->arp_cache[i];
            }
        }
    }

    if (oldest_resolved != NULL)
    {
        if (oldest_resolved->pending_packet != NULL)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, oldest_resolved->pending_packet);
            oldest_resolved->pending_packet = NULL;
        }
        oldest_resolved->needs_retransmit = false;
        oldest_resolved->state = xNET_ARP_ENTRY_FREE;
        return oldest_resolved;
    }

    // 3. Fallback: Evict the oldest pending entry (minimum remaining retry time relative to current_time)
    xNET_ARP_Entry_t *oldest_pending = NULL;
    int32_t min_retry = 0x7FFFFFFF;
    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        if (interface_ctx->arp_cache[i].state == xNET_ARP_ENTRY_PENDING)
        {
            int32_t remaining = (int32_t)(interface_ctx->arp_cache[i].retry_ms - current_time);
            if (remaining < min_retry)
            {
                min_retry = remaining;
                oldest_pending = &interface_ctx->arp_cache[i];
            }
        }
    }

    if (oldest_pending != NULL)
    {
        if (oldest_pending->pending_packet != NULL)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, oldest_pending->pending_packet);
            oldest_pending->pending_packet = NULL;
        }
        oldest_pending->needs_retransmit = false;
        oldest_pending->state = xNET_ARP_ENTRY_FREE;
        return oldest_pending;
    }

    return NULL;
}

static void
arp_cache_update(xNET_Interface_Context_t *interface_ctx, const xNET_IPv4_Address_t *ip_addr, const xNET_MAC_Address_t *mac_addr)
{
    // Search for existing entry for this IP
    xNET_ARP_Entry_t *entry = NULL;
    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        if ((interface_ctx->arp_cache[i].state != xNET_ARP_ENTRY_FREE) &&
            (memcmp(interface_ctx->arp_cache[i].ip_addr.addr, ip_addr->addr, 4) == 0))
        {
            entry = &interface_ctx->arp_cache[i];
            break;
        }
    }

    // If not found, allocate a new one (evicting if necessary)
    if (entry == NULL)
    {
        entry = arp_cache_allocate_entry(interface_ctx);
    }

    if (entry != NULL)
    {
        uint32_t current_time = interface_ctx->net_ctx->system_ticks;
        entry->ip_addr = *ip_addr;
        entry->mac_addr = *mac_addr;
        entry->state = xNET_ARP_ENTRY_RESOLVED;
        entry->timeout_ms = current_time + xNET_CONFIG_ARP_ENTRY_TIMEOUT_MS;
        entry->needs_retransmit = false;

        // If there was a pending packet waiting for this IP, transmit it using resolved MAC!
        if (entry->pending_packet != NULL)
        {
            (void)xNET_Ethernet_TX(interface_ctx, entry->pending_packet, mac_addr, xNET_ETHERTYPE_IPV4);
            entry->pending_packet = NULL;
        }
    }
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void xNET_ARP_Cache_Init(xNET_Interface_Context_t *interface_ctx)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if (interface_ctx == NULL)
    {
        return;
    }

    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        interface_ctx->arp_cache[i].state = xNET_ARP_ENTRY_FREE;
        interface_ctx->arp_cache[i].timeout_ms = 0U;
        interface_ctx->arp_cache[i].retry_ms = 0U;
        interface_ctx->arp_cache[i].retries_left = 0U;
        interface_ctx->arp_cache[i].needs_retransmit = false;
        interface_ctx->arp_cache[i].pending_packet = NULL;
        (void)memset(interface_ctx->arp_cache[i].ip_addr.addr, 0, 4);
        (void)memset(interface_ctx->arp_cache[i].mac_addr.addr, 0, 6);
    }
}

xRETURN_t xNET_ARP_Cache_Query(xNET_Interface_Context_t *interface_ctx, const xNET_IPv4_Address_t *ip_addr, xNET_MAC_Address_t *mac_addr)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(ip_addr != NULL, "ip_addr is NULL");
    xASSERT(mac_addr != NULL, "mac_addr is NULL");

    if ((interface_ctx == NULL) || (ip_addr == NULL) || (mac_addr == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        if ((interface_ctx->arp_cache[i].state == xNET_ARP_ENTRY_RESOLVED) &&
            (memcmp(interface_ctx->arp_cache[i].ip_addr.addr, ip_addr->addr, 4) == 0))
        {
            (void)memcpy(mac_addr->addr, interface_ctx->arp_cache[i].mac_addr.addr, 6);
            return xRETURN_xNET_OK;
        }
    }

    return xRETURN_xERR_xNET_NOT_FOUND;
}

xRETURN_t xNET_ARP_Send(xNET_Interface_Context_t *interface_ctx,
                        uint16_t operation,
                        const xNET_MAC_Address_t *target_mac,
                        const xNET_IPv4_Address_t *target_ip)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(target_ip != NULL, "target_ip is NULL");

    if ((interface_ctx == NULL) || (target_ip == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xNET_Packet_Buffer_t *packet = NULL;
    xRETURN_t ret = xNET_Packet_Alloc(interface_ctx->net_ctx, &packet);
    if (ret != xRETURN_xNET_OK)
    {
        return ret;
    }

    // Allocate safe headroom for Ethernet header (14 bytes), we start writing ARP body at offset 32
    packet->data_offset = 32U;
    packet->data_length = xNET_ARP_PACKET_SIZE;

    uint8_t *data = xNET_Packet_Get_Data(packet);
    if (data == NULL)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    // Hardware Type: Ethernet (1)
    data[0] = 0;
    data[1] = 1;
    // Protocol Type: IPv4 (0x0800)
    data[2] = 0x08;
    data[3] = 0x00;
    // Hardware Address Length: 6 (MAC)
    data[4] = 6;
    // Protocol Address Length: 4 (IP)
    data[5] = 4;
    // Operation
    data[6] = (uint8_t)((operation >> 8) & 0xFFU);
    data[7] = (uint8_t)(operation & 0xFFU);

    // Sender Hardware Address (SHA)
    (void)memcpy(&data[8], interface_ctx->mac_addr.addr, 6);
    // Sender Protocol Address (SPA)
    (void)memcpy(&data[14], interface_ctx->ip_addr.addr, 4);

    // Target Hardware Address (THA)
    if (operation == xNET_ARP_OP_REQUEST)
    {
        (void)memset(&data[18], 0, 6);
    }
    else
    {
        xASSERT(target_mac != NULL, "target_mac is NULL on Reply");
        if (target_mac == NULL)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
            return xRETURN_xERR_xNET_NULL_POINTER;
        }
        (void)memcpy(&data[18], target_mac->addr, 6);
    }

    // Target Protocol Address (TPA)
    (void)memcpy(&data[24], target_ip->addr, 4);

    // Destination Ethernet MAC address
    xNET_MAC_Address_t eth_dest_mac = {0};
    if (operation == xNET_ARP_OP_REQUEST)
    {
        (void)memset(eth_dest_mac.addr, 0xFF, 6); // Broadcast MAC
    }
    else
    {
        xASSERT(target_mac != NULL, "target_mac is NULL on Reply");
        if (target_mac == NULL)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
            return xRETURN_xERR_xNET_NULL_POINTER;
        }
        (void)memcpy(eth_dest_mac.addr, target_mac->addr, 6);
    }

    return xNET_Ethernet_TX(interface_ctx, packet, &eth_dest_mac, xNET_ETHERTYPE_ARP);
}

xRETURN_t xNET_ARP_Resolve(xNET_Interface_Context_t *interface_ctx, const xNET_IPv4_Address_t *ip_addr, xNET_Packet_Buffer_t *packet)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    xASSERT(ip_addr != NULL, "ip_addr is NULL");
    xASSERT(packet != NULL, "packet is NULL");

    if ((interface_ctx == NULL) || (ip_addr == NULL) || (packet == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    // 1. Check if broadcast destination IP
    bool is_broadcast =
        (ip_addr->addr[0] == 0xFFU) && (ip_addr->addr[1] == 0xFFU) && (ip_addr->addr[2] == 0xFFU) && (ip_addr->addr[3] == 0xFFU);
    if (is_broadcast)
    {
        xNET_MAC_Address_t broadcast_mac = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
        return xNET_Ethernet_TX(interface_ctx, packet, &broadcast_mac, xNET_ETHERTYPE_IPV4);
    }

    // 2. Check if multicast destination IP
    bool is_multicast = (ip_addr->addr[0] >= 224U) && (ip_addr->addr[0] <= 239U);
    if (is_multicast)
    {
        xNET_MAC_Address_t multicast_mac = {0};
        multicast_mac.addr[0] = 0x01U;
        multicast_mac.addr[1] = 0x00U;
        multicast_mac.addr[2] = 0x5EU;
        multicast_mac.addr[3] = ip_addr->addr[1] & 0x7FU;
        multicast_mac.addr[4] = ip_addr->addr[2];
        multicast_mac.addr[5] = ip_addr->addr[3];
        return xNET_Ethernet_TX(interface_ctx, packet, &multicast_mac, xNET_ETHERTYPE_IPV4);
    }

    // 3. Search cache
    xNET_MAC_Address_t resolved_mac = {0};
    xRETURN_t ret = xNET_ARP_Cache_Query(interface_ctx, ip_addr, &resolved_mac);
    if (ret == xRETURN_xNET_OK)
    {
        return xNET_Ethernet_TX(interface_ctx, packet, &resolved_mac, xNET_ETHERTYPE_IPV4);
    }

    // 4. Cache entry not found or pending. Look for existing entry to buffer this packet.
    xNET_ARP_Entry_t *entry = NULL;
    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        if ((interface_ctx->arp_cache[i].state != xNET_ARP_ENTRY_FREE) &&
            (memcmp(interface_ctx->arp_cache[i].ip_addr.addr, ip_addr->addr, 4) == 0))
        {
            entry = &interface_ctx->arp_cache[i];
            break;
        }
    }

    if (entry != NULL)
    {
        // Entry is PENDING. Buffer the packet and release the previous pending packet.
        if (entry->pending_packet != NULL)
        {
            (void)xNET_Packet_Release(interface_ctx->net_ctx, entry->pending_packet);
        }
        entry->pending_packet = packet;
        return xRETURN_xNET_OK;
    }

    // 5. No entry found. Allocate a new entry in PENDING state.
    entry = arp_cache_allocate_entry(interface_ctx);
    if (entry == NULL)
    {
        (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);
        return xRETURN_xERR_xNET_NO_PACKET_BUFFER;
    }

    uint32_t current_time = interface_ctx->net_ctx->system_ticks;
    entry->ip_addr = *ip_addr;
    entry->state = xNET_ARP_ENTRY_PENDING;
    entry->timeout_ms = current_time + xNET_CONFIG_ARP_ENTRY_TIMEOUT_MS;
    entry->retry_ms = current_time + xNET_CONFIG_ARP_RETRY_TIMEOUT_MS;
    entry->retries_left = xNET_CONFIG_ARP_MAX_RETRIES;
    entry->needs_retransmit = false;
    entry->pending_packet = packet;

    // Send initial ARP Request
    (void)xNET_ARP_Send(interface_ctx, xNET_ARP_OP_REQUEST, NULL, ip_addr);

    return xRETURN_xNET_OK;
}

xRETURN_t xNET_ARP_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet)
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

    if (length < xNET_ARP_PACKET_SIZE)
    {
        return xRETURN_xERR_xNET_INVALID_PACKET;
    }

    uint16_t htype = ((uint16_t)data[0] << 8) | data[1];
    uint16_t ptype = ((uint16_t)data[2] << 8) | data[3];
    uint8_t hlen = data[4];
    uint8_t plen = data[5];
    uint16_t oper = ((uint16_t)data[6] << 8) | data[7];

    // Validate headers (Ethernet + IPv4 mapping sizes)
    if ((htype != 1U) || (ptype != 0x0800U) || (hlen != 6U) || (plen != 4U))
    {
        return xRETURN_xERR_xNET_INVALID_PACKET;
    }

    xNET_MAC_Address_t sha = {0};
    xNET_IPv4_Address_t spa = {0};
    xNET_MAC_Address_t tha = {0};
    xNET_IPv4_Address_t tpa = {0};

    (void)memcpy(sha.addr, &data[8], 6);
    (void)memcpy(spa.addr, &data[14], 4);
    (void)memcpy(tha.addr, &data[18], 6);
    (void)memcpy(tpa.addr, &data[24], 4);

    // Verify if the target IP matches our interface's IP
    bool is_for_us = memcmp(tpa.addr, interface_ctx->ip_addr.addr, 4) == 0;

    if (is_for_us)
    {
        // Update/insert sender mapping to cache
        arp_cache_update(interface_ctx, &spa, &sha);

        if (oper == xNET_ARP_OP_REQUEST)
        {
            // Send ARP Reply
            (void)xNET_ARP_Send(interface_ctx, xNET_ARP_OP_REPLY, &sha, &spa);
        }
    }

    // Release packet (ownership consumed)
    (void)xNET_Packet_Release(interface_ctx->net_ctx, packet);

    return xRETURN_xNET_OK;
}

void xNET_ARP_Tick(xNET_Interface_Context_t *interface_ctx, uint32_t elapsed_ms)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if (interface_ctx == NULL)
    {
        return;
    }

    (void)elapsed_ms;
    uint32_t current_time = interface_ctx->net_ctx->system_ticks;

    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        xNET_ARP_Entry_t *entry = &interface_ctx->arp_cache[i];

        if (entry->state == xNET_ARP_ENTRY_PENDING)
        {
            if (xNET_Time_After_Or_Equal(current_time, entry->retry_ms))
            {
                if (entry->retries_left > 0)
                {
                    entry->retries_left--;
                    entry->retry_ms = current_time + xNET_CONFIG_ARP_RETRY_TIMEOUT_MS;
                    entry->needs_retransmit = true;
                }
                else
                {
                    // Retries exhausted - clear entry and release pending packet buffer
                    if (entry->pending_packet != NULL)
                    {
                        (void)xNET_Packet_Release(interface_ctx->net_ctx, entry->pending_packet);
                        entry->pending_packet = NULL;
                    }
                    entry->needs_retransmit = false;
                    entry->state = xNET_ARP_ENTRY_FREE;
                }
            }
        }
        else if (entry->state == xNET_ARP_ENTRY_RESOLVED)
        {
            if (xNET_Time_After_Or_Equal(current_time, entry->timeout_ms))
            {
                // Expired
                entry->needs_retransmit = false;
                entry->state = xNET_ARP_ENTRY_FREE;
            }
        }
        else
        {
            // Entry is FREE, do nothing.
        }
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
