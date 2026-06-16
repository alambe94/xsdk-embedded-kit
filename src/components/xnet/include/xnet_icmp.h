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

// @file xnet_icmp.h
// @brief Internet Control Message Protocol (ICMP) parser and Echo handler declarations.
//

#ifndef XNET_ICMP_H
#define XNET_ICMP_H

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
#define xNET_ICMP_TYPE_ECHO_REPLY   0U
#define xNET_ICMP_TYPE_ECHO_REQUEST 8U

#define xNET_ICMP_HEADER_SIZE 8U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xNET_ICMP_RX(xNET_Interface_Context_t *interface_ctx, xNET_Packet_Buffer_t *packet, const xNET_IPv4_Address_t *src_ip);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_ICMP_H
// EOF /////////////////////////////////////////////////////////////////////////////
