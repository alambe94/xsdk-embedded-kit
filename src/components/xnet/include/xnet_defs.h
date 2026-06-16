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

// @file xnet_defs.h
// @brief Common definitions, constants, and helper macros for xNET module.
//

#ifndef XNET_DEFS_H
#define XNET_DEFS_H

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
#include "xbytes.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xNET_ETHERNET_MTU            1500U
#define xNET_ETHERNET_HEADER_SIZE    14U
#define xNET_ETHERNET_MIN_FRAME_SIZE 60U
#define xNET_IPV4_HEADER_MIN_SIZE    20U
#define xNET_UDP_HEADER_SIZE         8U
#define xNET_MAC_ADDRESS_SIZE        6U
#define xNET_IPV4_ADDRESS_SIZE       4U

#define xNET_ARP_HARDWARE_ETHERNET 1U
#define xNET_ARP_PROTOCOL_IPV4     0x0800U

#define xNET_ETHERTYPE_IPV4 0x0800U
#define xNET_ETHERTYPE_ARP  0x0806U

// Endianness swapping wrappers
#define xNET_HTONS(x) xCPU_TO_BE16(x)
#define xNET_NTOHS(x) xBE16_TO_CPU(x)
#define xNET_HTONL(x) xCPU_TO_BE32(x)
#define xNET_NTOHL(x) xBE32_TO_CPU(x)

// RX Offload Metadata Flags
#define xNET_RX_FLAG_NONE                0x00U
#define xNET_RX_FLAG_IP_CHECKSUM_VALID   0x01U /**< HW verified IP checksum and it was valid */
#define xNET_RX_FLAG_IP_CHECKSUM_INVALID 0x02U /**< HW verified IP checksum and it failed */
#define xNET_RX_FLAG_L4_CHECKSUM_VALID   0x04U /**< HW verified L4 (UDP/TCP) checksum and it was valid */
#define xNET_RX_FLAG_L4_CHECKSUM_INVALID 0x08U /**< HW verified L4 checksum and it failed */

// TX Offload Metadata Flags
#define xNET_TX_FLAG_NONE      0x00U
#define xNET_TX_FLAG_CSUM_IP   0x01U /**< HW needs to calculate IPv4 header checksum */
#define xNET_TX_FLAG_CSUM_UDP  0x02U /**< HW needs to calculate UDP checksum */
#define xNET_TX_FLAG_CSUM_TCP  0x04U /**< HW needs to calculate TCP checksum */
#define xNET_TX_FLAG_CSUM_ICMP 0x08U /**< HW needs to calculate ICMP checksum */

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct
    {
        uint8_t addr[xNET_MAC_ADDRESS_SIZE];
    } xNET_MAC_Address_t;

    typedef struct
    {
        uint8_t addr[xNET_IPV4_ADDRESS_SIZE];
    } xNET_IPv4_Address_t;

    // Checksum Capabilities
    typedef enum
    {
        xNET_CHECKSUM_CAP_NONE = 0x00U,
        xNET_CHECKSUM_CAP_IP_TX = 0x01U,   /**< HW calculates IPv4 header checksum on TX */
        xNET_CHECKSUM_CAP_IP_RX = 0x02U,   /**< HW verifies IPv4 header checksum on RX */
        xNET_CHECKSUM_CAP_UDP_TX = 0x04U,  /**< HW calculates UDP checksum on TX */
        xNET_CHECKSUM_CAP_UDP_RX = 0x08U,  /**< HW verifies UDP checksum on RX */
        xNET_CHECKSUM_CAP_TCP_TX = 0x10U,  /**< HW calculates TCP checksum on TX */
        xNET_CHECKSUM_CAP_TCP_RX = 0x20U,  /**< HW verifies TCP checksum on RX */
        xNET_CHECKSUM_CAP_ICMP_TX = 0x40U, /**< HW calculates ICMP checksum on TX */
        xNET_CHECKSUM_CAP_ICMP_RX = 0x80U, /**< HW verifies ICMP checksum on RX */
    } xNET_Checksum_Cap_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    uint16_t xNET_Checksum_Calculate(const void *data, uint32_t length);
    uint16_t xNET_Checksum_Calculate_Pseudo(const void *data,
                                            uint32_t length,
                                            const xNET_IPv4_Address_t *src,
                                            const xNET_IPv4_Address_t *dest,
                                            uint8_t protocol,
                                            uint16_t proto_len);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
