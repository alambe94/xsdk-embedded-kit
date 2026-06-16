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

// @file xnet_interface.h
// @brief Network interface declarations and driver contracts for the xNET module.
//

#ifndef XNET_INTERFACE_H
#define XNET_INTERFACE_H

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
#include "xnet_config.h"
#include "xnet_defs.h"
#include "xnet_return.h"
#include "xnet_arp.h"
#include "xnet_dhcp.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum
    {
        xNET_INTERFACE_STATE_DOWN = 0,
        xNET_INTERFACE_STATE_UP,
    } xNET_Interface_State_t;

    // Forward declarations
    typedef struct xNET_Context_t xNET_Context_t;
    typedef struct xNET_Interface_Ops_t xNET_Interface_Ops_t;
    typedef struct xNET_Interface_Context_t xNET_Interface_Context_t;

    struct xNET_Interface_Ops_t
    {
        xRETURN_t (*transmit)(void *driver_ctx, const uint8_t *packet, uint32_t length, uint32_t tx_flags);

        xRETURN_t (*poll)(void *driver_ctx);

        xRETURN_t (*set_multicast_filter)(void *driver_ctx, const xNET_MAC_Address_t *mac_addr, bool is_enabled);

        xRETURN_t (*flush)(void *driver_ctx);
    };

    struct xNET_Interface_Context_t
    {
        xNET_Context_t *net_ctx;
        xNET_MAC_Address_t mac_addr;
        xNET_IPv4_Address_t ip_addr;
        xNET_IPv4_Address_t netmask;
        xNET_IPv4_Address_t gateway;
        xNET_IPv4_Address_t dns_primary;
        xNET_IPv4_Address_t dns_secondary;

        xNET_Interface_State_t state;
        uint32_t checksum_caps;

        xNET_ARP_Entry_t arp_cache[xNET_CONFIG_ARP_CACHE_SIZE];
        xNET_DHCP_Context_t dhcp_ctx;

        const xNET_Interface_Ops_t *ops;
        void *driver_ctx;

        xNET_Interface_Context_t *next;
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xNET_Interface_Link_Set(xNET_Interface_Context_t *interface_ctx, bool is_link_up);
    xRETURN_t xNET_Interface_RX_Frame(xNET_Interface_Context_t *interface_ctx, const uint8_t *frame, uint32_t length, uint32_t rx_flags);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_INTERFACE_H
// EOF /////////////////////////////////////////////////////////////////////////////
