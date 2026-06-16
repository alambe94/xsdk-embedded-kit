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

// @file xnet_ipv4.h
// @brief Internet Protocol version 4 (IPv4) parser and output path declarations.
//

#ifndef XNET_IPV4_H
#define XNET_IPV4_H

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
#include "xnet_interface.h"
#include "xnet_packet.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xNET_IPV4_PROTOCOL_ICMP 1U
#define xNET_IPV4_PROTOCOL_UDP  17U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xNET_IPv4_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet);
    xRETURN_t xNET_IPv4_TX(xNET_Interface_Context_t *interface_ctx,
                           xNET_Packet_Buffer_t *packet,
                           const xNET_IPv4_Address_t *dest_ip,
                           uint8_t protocol);
    xRETURN_t xNET_IPv4_Config_Static(xNET_Interface_Context_t *interface_ctx,
                                      const xNET_IPv4_Address_t *ip_addr,
                                      const xNET_IPv4_Address_t *netmask,
                                      const xNET_IPv4_Address_t *gateway);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_IPV4_H
// EOF /////////////////////////////////////////////////////////////////////////////
