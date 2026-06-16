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

// @file xnet_arp.h
// @brief ARP protocol and cache types and function prototypes.
//

#ifndef XNET_ARP_H
#define XNET_ARP_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xnet_defs.h"
#include "xnet_return.h"
#include "xnet_packet.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xNET_ARP_OP_REQUEST  1U
#define xNET_ARP_OP_REPLY    2U
#define xNET_ARP_PACKET_SIZE 28U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum
    {
        xNET_ARP_ENTRY_FREE = 0,
        xNET_ARP_ENTRY_PENDING,
        xNET_ARP_ENTRY_RESOLVED
    } xNET_ARP_Entry_State_t;

    typedef struct
    {
        xNET_IPv4_Address_t ip_addr;
        xNET_MAC_Address_t mac_addr;
        xNET_ARP_Entry_State_t state;
        uint32_t timeout_ms;
        uint32_t retry_ms;
        uint8_t retries_left;
        bool needs_retransmit;
        xNET_Packet_Buffer_t *pending_packet;
    } xNET_ARP_Entry_t;

    // Forward declaration of interface context to avoid circular dependencies
    typedef struct xNET_Interface_Context_t xNET_Interface_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Initialize the ARP cache of an interface.
     *
     * @param interface_ctx Pointer to the interface context.
     */
    void xNET_ARP_Cache_Init(xNET_Interface_Context_t *interface_ctx);

    /**
     * @brief Query the ARP cache for a MAC address mapping.
     *
     * @param interface_ctx Pointer to the interface context.
     * @param ip_addr Pointer to the IPv4 address to lookup.
     * @param mac_addr Pointer to store the resolved MAC address.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t
    xNET_ARP_Cache_Query(xNET_Interface_Context_t *interface_ctx, const xNET_IPv4_Address_t *ip_addr, xNET_MAC_Address_t *mac_addr);

    /**
     * @brief Resolve a MAC address. If not resolved, queues the packet and sends ARP request.
     *
     * @param interface_ctx Pointer to the interface context.
     * @param ip_addr Pointer to the destination IPv4 address.
     * @param packet Pointer to the packet buffer waiting to be sent.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t xNET_ARP_Resolve(xNET_Interface_Context_t *interface_ctx, const xNET_IPv4_Address_t *ip_addr, xNET_Packet_Buffer_t *packet);

    /**
     * @brief Process an incoming ARP packet.
     *
     * @param interface_ctx Pointer to the interface context.
     * @param packet Pointer to the packet buffer containing the ARP frame.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t xNET_ARP_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet);

    /**
     * @brief Send an ARP Request or Reply.
     *
     * @param interface_ctx Pointer to the interface context.
     * @param operation ARP operation code (Request or Reply).
     * @param target_mac Pointer to the target MAC address (for Reply, or NULL/zeros for Request).
     * @param target_ip Pointer to the target IPv4 address.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t xNET_ARP_Send(xNET_Interface_Context_t *interface_ctx,
                            uint16_t operation,
                            const xNET_MAC_Address_t *target_mac,
                            const xNET_IPv4_Address_t *target_ip);

    /**
     * @brief Tick update for ARP cache timeouts and retries.
     *
     * @param interface_ctx Pointer to the interface context.
     * @param elapsed_ms Elapsed milliseconds since last call.
     */
    void xNET_ARP_Tick(xNET_Interface_Context_t *interface_ctx, uint32_t elapsed_ms);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_ARP_H
// EOF /////////////////////////////////////////////////////////////////////////////
