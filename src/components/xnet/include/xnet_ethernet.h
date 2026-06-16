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

// @file xnet_ethernet.h
// @brief Ethernet II protocol parser and builder interfaces.
//

#ifndef XNET_ETHERNET_H
#define XNET_ETHERNET_H

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
#include "xnet_interface.h"
#include "xnet_packet.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Checks if a MAC address is a broadcast address (FF:FF:FF:FF:FF:FF).
     *
     * @param mac Pointer to the MAC address to check.
     * @return true if the MAC address is broadcast, false otherwise.
     */
    bool xNET_Ethernet_Is_Broadcast(const xNET_MAC_Address_t *mac);

    /**
     * @brief Checks if a MAC address is a multicast address.
     *
     * @param mac Pointer to the MAC address to check.
     * @return true if the MAC address is multicast, false otherwise.
     */
    bool xNET_Ethernet_Is_Multicast(const xNET_MAC_Address_t *mac);

    /**
     * @brief Parse raw Ethernet II header fields from a buffer.
     *
     * @param data Pointer to the start of the Ethernet II frame.
     * @param length Total length of the frame data.
     * @param dest Pointer to store the destination MAC address.
     * @param src Pointer to store the source MAC address.
     * @param ethertype Pointer to store the extracted and endian-swapped EtherType.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t
    xNET_Ethernet_Parse(const uint8_t *data, uint32_t length, xNET_MAC_Address_t *dest, xNET_MAC_Address_t *src, uint16_t *ethertype);

    /**
     * @brief Build raw Ethernet II header fields into a buffer.
     *
     * @param data Pointer to the buffer where header should be written.
     * @param length Total length of the available buffer space.
     * @param dest Pointer to the destination MAC address.
     * @param src Pointer to the source MAC address.
     * @param ethertype The EtherType to set.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t
    xNET_Ethernet_Build(uint8_t *data, uint32_t length, const xNET_MAC_Address_t *dest, const xNET_MAC_Address_t *src, uint16_t ethertype);

    /**
     * @brief Process an incoming Ethernet II packet buffer.
     *
     * @param interface_ctx Pointer to the interface context.
     * @param packet Pointer to the allocated packet buffer containing the frame.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t xNET_Ethernet_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet);

    /**
     * @brief Construct and transmit an outgoing Ethernet II packet buffer.
     *
     * @param interface_ctx Pointer to the interface context.
     * @param packet Pointer to the packet buffer containing the payload.
     * @param dest_mac Pointer to the destination MAC address.
     * @param ethertype The EtherType of the payload.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t xNET_Ethernet_TX(xNET_Interface_Context_t *interface_ctx,
                               xNET_Packet_Buffer_t *packet,
                               const xNET_MAC_Address_t *dest_mac,
                               uint16_t ethertype);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_ETHERNET_H
// EOF /////////////////////////////////////////////////////////////////////////////
