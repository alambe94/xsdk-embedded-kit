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

// @file xnet_core.h
// @brief Core context structures and entry points for the xNET module.
//

#ifndef XNET_CORE_H
#define XNET_CORE_H

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
#include "xnet_config.h"
#include "xnet_return.h"
#include "xnet_interface.h"
#include "xnet_packet.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct
    {
        uint8_t *packet_pool_buffer;
        uint32_t packet_pool_buffer_size;
    } xNET_Config_t;

    struct xNET_UDP_Context_t;

    typedef struct xNET_Context_t
    {
        bool is_initialized;
        xNET_Config_t config;
        xNET_Interface_Context_t *interface_list;
        uint32_t interface_count;
        xNET_Packet_Buffer_t packet_pool[xNET_CONFIG_PACKET_POOL_SIZE];
        struct xNET_UDP_Context_t *udp_list;
        uint32_t system_ticks;
    } xNET_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xNET_Init(xNET_Context_t *net_ctx, const xNET_Config_t *net_config);
    xRETURN_t xNET_Interface_Add(xNET_Context_t *net_ctx, xNET_Interface_Context_t *interface_ctx);
    xRETURN_t xNET_Process(xNET_Context_t *net_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_CORE_H
// EOF /////////////////////////////////////////////////////////////////////////////
