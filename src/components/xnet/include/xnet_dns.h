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

// @file xnet_dns.h
// @brief User Datagram Protocol (UDP) based DNS client.
//

#ifndef XNET_DNS_H
#define XNET_DNS_H

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

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Forward declaration of core networking context
    typedef struct xNET_Context_t xNET_Context_t;

    // Forward declaration of DNS context
    typedef struct xNET_DNS_Context_t xNET_DNS_Context_t;

    /**
 * @brief DNS result callback type.
 *
 * @param dns_ctx Pointer to the DNS context.
 * @param hostname Pointer to the queried hostname string.
 * @param ip_addr Pointer to the resolved IPv4 address, or NULL on failure.
 * @param result Status result of the query.
 * @return xRETURN_t Status code.
 */
    typedef xRETURN_t (*xNET_DNS_Result_Callback_t)(xNET_DNS_Context_t *dns_ctx,
                                                    const char *hostname,
                                                    const xNET_IPv4_Address_t *ip_addr,
                                                    xRETURN_t result);

    typedef enum
    {
        xNET_DNS_STATE_IDLE = 0,
        xNET_DNS_STATE_QUERYING,
        xNET_DNS_STATE_SUCCESS,
        xNET_DNS_STATE_FAILED
    } xNET_DNS_State_t;

    struct xNET_DNS_Context_t
    {
        xNET_Context_t *net_ctx;
        xNET_DNS_State_t state;
        xNET_UDP_Context_t udp_socket;
        uint16_t transaction_id;
        uint32_t retry_elapsed_ms;
        uint32_t current_retry_timeout;
        uint8_t retry_count;
        char hostname[256];
        xNET_DNS_Result_Callback_t on_result;
        xNET_IPv4_Address_t dns_server;
        xNET_IPv4_Address_t resolved_ip;
        xRETURN_t result_status;
        bool is_active;
    };

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    /**
 * @brief Start an asynchronous DNS A-record query for a hostname.
 *
 * @param net_ctx Pointer to the xNET context.
 * @param dns_ctx Pointer to the caller-allocated DNS context.
 * @param hostname Hostname to query.
 * @param on_result Callback to execute when result is available or query fails.
 * @return xRETURN_t xRETURN_xNET_OK on success, or an error code.
 */
    xRETURN_t
    xNET_DNS_Query_A(xNET_Context_t *net_ctx, xNET_DNS_Context_t *dns_ctx, const char *hostname, xNET_DNS_Result_Callback_t on_result);

    /**
 * @brief Tick update for DNS query timeouts and retries.
 *
 * @param dns_ctx Pointer to the DNS context.
 * @param elapsed_ms Elapsed milliseconds since last call.
 */
    void xNET_DNS_Tick(xNET_DNS_Context_t *dns_ctx, uint32_t elapsed_ms);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XNET_DNS_H
// EOF /////////////////////////////////////////////////////////////////////////////
