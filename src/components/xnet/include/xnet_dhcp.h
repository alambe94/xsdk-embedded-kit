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

// @file xnet_dhcp.h
// @brief User Datagram Protocol (UDP) based DHCP client.
//

#ifndef XNET_DHCP_H
#define XNET_DHCP_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xnet_defs.h"
#include "xnet_return.h"
#include "xnet_udp.h"

// MACROS //////////////////////////////////////////////////////////////////////
#define xNET_DHCP_CLIENT_PORT 68U
#define xNET_DHCP_SERVER_PORT 67U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef enum
    {
        xNET_DHCP_STATE_INIT = 0,
        xNET_DHCP_STATE_SELECTING,
        xNET_DHCP_STATE_REQUESTING,
        xNET_DHCP_STATE_BOUND,
        xNET_DHCP_STATE_RENEWING,
        xNET_DHCP_STATE_REBINDING,
        xNET_DHCP_STATE_FAILED
    } xNET_DHCP_State_t;

    typedef struct
    {
        xNET_DHCP_State_t state;
        xNET_UDP_Context_t udp_socket;
        uint32_t xid;
        uint32_t lease_time;             // in seconds
        uint32_t t1;                     // in seconds
        uint32_t t2;                     // in seconds
        uint32_t lease_elapsed_total_ms; // ms since BOUND
        uint32_t t1_elapsed_ms;          // ms since BOUND
        uint32_t t2_elapsed_ms;          // ms since BOUND
        uint32_t retry_elapsed_ms;       // ms since last transmit
        uint32_t current_retry_timeout;  // ms (doubles: 4000, 8000, etc.)
        uint8_t retry_count;
        xNET_IPv4_Address_t offered_ip;
        xNET_IPv4_Address_t server_ip;
        xNET_IPv4_Address_t netmask;
        xNET_IPv4_Address_t gateway;
        xNET_IPv4_Address_t dns_primary;
        xNET_IPv4_Address_t dns_secondary;
        bool is_active;
    } xNET_DHCP_Context_t;

    // Forward declaration to avoid circular dependencies
    typedef struct xNET_Interface_Context_t xNET_Interface_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
     * @brief Start the DHCP client on a given interface.
     *
     * @param interface_ctx Pointer to the interface context.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t xNET_DHCP_Start(xNET_Interface_Context_t *interface_ctx);

    /**
     * @brief Stop the DHCP client on a given interface.
     *
     * @param interface_ctx Pointer to the interface context.
     * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
     */
    xRETURN_t xNET_DHCP_Stop(xNET_Interface_Context_t *interface_ctx);

    /**
     * @brief Tick update for the DHCP client.
     *
     * @param interface_ctx Pointer to the interface context.
     * @param elapsed_ms Elapsed milliseconds since last call.
     */
    void xNET_DHCP_Tick(xNET_Interface_Context_t *interface_ctx, uint32_t elapsed_ms);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_DHCP_H
// EOF /////////////////////////////////////////////////////////////////////////////
