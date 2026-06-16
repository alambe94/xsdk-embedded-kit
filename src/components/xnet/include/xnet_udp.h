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

// @file xnet_udp.h
// @brief User Datagram Protocol (UDP) declarations.
//

#ifndef XNET_UDP_H
#define XNET_UDP_H

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
#include "xnet_packet.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    struct xNET_Context_t;
    typedef struct xNET_Interface_Context_t xNET_Interface_Context_t;
    typedef struct xNET_UDP_Context_t xNET_UDP_Context_t;

    typedef xRETURN_t (*xNET_UDP_Receive_Callback_t)(xNET_UDP_Context_t *udp_ctx,
                                                     const xNET_IPv4_Address_t *remote_addr,
                                                     uint16_t remote_port,
                                                     const uint8_t *data,
                                                     uint32_t length);

    struct xNET_UDP_Context_t
    {
        struct xNET_Context_t *net_ctx;
        uint16_t local_port;
        xNET_UDP_Receive_Callback_t on_received;
        xNET_UDP_Context_t *next;
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xNET_UDP_Open(struct xNET_Context_t *net_ctx,
                            xNET_UDP_Context_t *udp_ctx,
                            uint16_t local_port,
                            xNET_UDP_Receive_Callback_t on_received);

    xRETURN_t xNET_UDP_Close(xNET_UDP_Context_t *udp_ctx);

    xRETURN_t xNET_UDP_Send_To(xNET_UDP_Context_t *udp_ctx,
                               const xNET_IPv4_Address_t *remote_addr,
                               uint16_t remote_port,
                               const uint8_t *data,
                               uint32_t length);

    xRETURN_t xNET_UDP_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet, const xNET_IPv4_Address_t *src_ip);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_UDP_H
// EOF /////////////////////////////////////////////////////////////////////////////
