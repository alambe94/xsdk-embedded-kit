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

// @file xnet_dhcp.c
// @brief DHCP client implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xnet_dhcp.h"
#include "xnet_core.h"
#include "xnet_interface.h"
#include "xnet_packet.h"
#include "xnet_udp.h"
#include "xnet_arp.h"
#include "xassert.h"

#include "xnet_log.h"

// MACROS /////////////////////////////////////////////////////////////////////////
#define xNET_DHCP_OP_BOOTREQUEST 1U
#define xNET_DHCP_OP_BOOTREPLY   2U
#define xNET_DHCP_HTYPE_ETHERNET 1U
#define xNET_DHCP_HLEN_ETHERNET  6U

#define xNET_DHCP_MAGIC_COOKIE 0x63825363U

// TYPES //////////////////////////////////////////////////////////////////////////

// VARIABLES //////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static xRETURN_t dhcp_send_message(xNET_Interface_Context_t *interface_ctx, uint8_t msg_type);

static xRETURN_t dhcp_rx_callback(xNET_UDP_Context_t *udp_ctx,
                                  const xNET_IPv4_Address_t *remote_addr,
                                  uint16_t remote_port,
                                  const uint8_t *data,
                                  uint32_t length);

static bool dhcp_validate_rx_packet(const xNET_Interface_Context_t *interface_ctx,
                                    const xNET_DHCP_Context_t *dhcp_ctx,
                                    const uint8_t *data,
                                    uint32_t length);

static void dhcp_parse_dns_option(uint8_t opt_len, const uint8_t *val, xNET_DHCP_Context_t *dhcp_ctx);

static void dhcp_parse_network_option(uint8_t opt_code, uint8_t opt_len, const uint8_t *val, xNET_DHCP_Context_t *dhcp_ctx);

static void dhcp_parse_time_option(uint8_t opt_code,
                                   uint8_t opt_len,
                                   const uint8_t *val,
                                   uint8_t *msg_type,
                                   uint32_t *lease_time,
                                   uint32_t *t1,
                                   uint32_t *t2);

static void dhcp_parse_options(const uint8_t *data,
                               uint32_t length,
                               xNET_DHCP_Context_t *dhcp_ctx,
                               uint8_t *msg_type,
                               uint32_t *lease_time,
                               uint32_t *t1,
                               uint32_t *t2);

static void
dhcp_handle_selecting_state(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx, uint8_t msg_type, const uint8_t *data);

static void dhcp_handle_active_state(xNET_Interface_Context_t *interface_ctx,
                                     xNET_DHCP_Context_t *dhcp_ctx,
                                     uint8_t msg_type,
                                     const uint8_t *data,
                                     uint32_t lease_time,
                                     uint32_t t1,
                                     uint32_t t2);

static void dhcp_tick_retries(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx, uint32_t elapsed_ms);

static void dhcp_tick_lease(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx, uint32_t elapsed_ms);

static void dhcp_handle_retry_exhausted(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx);

static void dhcp_handle_lease_expired(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static xRETURN_t dhcp_send_message(xNET_Interface_Context_t *interface_ctx, uint8_t msg_type)
{
    xNET_DHCP_Context_t *dhcp_ctx = &interface_ctx->dhcp_ctx;
    uint8_t payload[300];
    (void)memset(payload, 0, sizeof(payload));

    // Basic Header
    payload[0] = xNET_DHCP_OP_BOOTREQUEST;
    payload[1] = xNET_DHCP_HTYPE_ETHERNET;
    payload[2] = xNET_DHCP_HLEN_ETHERNET;
    payload[3] = 0U; // hops

    // xid
    uint32_t xid_be = xNET_HTONL(dhcp_ctx->xid);
    (void)memcpy(&payload[4], &xid_be, 4);

    // secs = 0
    payload[8] = 0U;
    payload[9] = 0U;

    // flags = 0x8000 (Broadcast)
    payload[10] = 0x80U;
    payload[11] = 0x00U;

    // ciaddr (populated only in RENEWING or REBINDING)
    if ((dhcp_ctx->state == xNET_DHCP_STATE_RENEWING) || (dhcp_ctx->state == xNET_DHCP_STATE_REBINDING))
    {
        (void)memcpy(&payload[12], interface_ctx->ip_addr.addr, 4);
    }

    // chaddr
    (void)memcpy(&payload[28], interface_ctx->mac_addr.addr, 6);

    // magic cookie
    uint32_t cookie_be = xNET_HTONL(xNET_DHCP_MAGIC_COOKIE);
    (void)memcpy(&payload[236], &cookie_be, 4);

    // Options
    uint32_t opt_idx = 240U;

    // Option 53: Message Type
    payload[opt_idx] = 53U;
    payload[opt_idx + 1U] = 1U;
    payload[opt_idx + 2U] = msg_type;
    opt_idx += 3U;

    if (msg_type == 3U) // DHCPREQUEST
    {
        // For SELECTING and REQUESTING, include Option 50 & 54
        if ((dhcp_ctx->state == xNET_DHCP_STATE_SELECTING) || (dhcp_ctx->state == xNET_DHCP_STATE_REQUESTING))
        {
            // Option 50: Requested IP
            payload[opt_idx] = 50U;
            payload[opt_idx + 1U] = 4U;
            (void)memcpy(&payload[opt_idx + 2U], dhcp_ctx->offered_ip.addr, 4);
            opt_idx += 6U;

            // Option 54: Server ID
            payload[opt_idx] = 54U;
            payload[opt_idx + 1U] = 4U;
            (void)memcpy(&payload[opt_idx + 2U], dhcp_ctx->server_ip.addr, 4);
            opt_idx += 6U;
        }
    }

    // Option 55: Parameter Request List
    payload[opt_idx] = 55U;
    payload[opt_idx + 1U] = 4U;
    payload[opt_idx + 2U] = 1U;  // Subnet Mask
    payload[opt_idx + 3U] = 3U;  // Router (Gateway)
    payload[opt_idx + 4U] = 6U;  // DNS Server
    payload[opt_idx + 5U] = 15U; // Domain Name
    opt_idx += 6U;

    // Option 255: End
    payload[opt_idx] = 255U;

    // Transmit
    xNET_IPv4_Address_t dest_ip;
    if (dhcp_ctx->state == xNET_DHCP_STATE_RENEWING)
    {
        dest_ip = dhcp_ctx->server_ip;
    }
    else
    {
        dest_ip = (xNET_IPv4_Address_t){{255, 255, 255, 255}};
    }

    return xNET_UDP_Send_To(&dhcp_ctx->udp_socket, &dest_ip, xNET_DHCP_SERVER_PORT, payload, sizeof(payload));
}

static void dhcp_parse_dns_option(uint8_t opt_len, const uint8_t *val, xNET_DHCP_Context_t *dhcp_ctx)
{
    if (opt_len >= 4U)
    {
        (void)memcpy(dhcp_ctx->dns_primary.addr, val, 4U);
    }
    if (opt_len >= 8U)
    {
        (void)memcpy(dhcp_ctx->dns_secondary.addr, &val[4], 4U);
    }
}

static void dhcp_parse_network_option(uint8_t opt_code, uint8_t opt_len, const uint8_t *val, xNET_DHCP_Context_t *dhcp_ctx)
{
    switch (opt_code)
    {
    case 1U: // Subnet Mask
        if (opt_len == 4U)
        {
            (void)memcpy(dhcp_ctx->netmask.addr, val, 4U);
        }
        break;
    case 3U: // Router
        if (opt_len >= 4U)
        {
            (void)memcpy(dhcp_ctx->gateway.addr, val, 4U);
        }
        break;
    case 6U: // DNS
        dhcp_parse_dns_option(opt_len, val, dhcp_ctx);
        break;
    case 54U: // Server ID
        if (opt_len == 4U)
        {
            (void)memcpy(dhcp_ctx->server_ip.addr, val, 4U);
        }
        break;
    default:
        break;
    }
}

static void dhcp_parse_time_option(uint8_t opt_code,
                                   uint8_t opt_len,
                                   const uint8_t *val,
                                   uint8_t *msg_type,
                                   uint32_t *lease_time,
                                   uint32_t *t1,
                                   uint32_t *t2)
{
    switch (opt_code)
    {
    case 53U: // Message Type
        if (opt_len == 1U)
        {
            *msg_type = val[0];
        }
        break;
    case 51U: // Lease Time
        if (opt_len == 4U)
        {
            *lease_time = ((uint32_t)val[0] << 24U) | ((uint32_t)val[1] << 16U) | ((uint32_t)val[2] << 8U) | val[3];
        }
        break;
    case 58U: // T1
        if (opt_len == 4U)
        {
            *t1 = ((uint32_t)val[0] << 24U) | ((uint32_t)val[1] << 16U) | ((uint32_t)val[2] << 8U) | val[3];
        }
        break;
    case 59U: // T2
        if (opt_len == 4U)
        {
            *t2 = ((uint32_t)val[0] << 24U) | ((uint32_t)val[1] << 16U) | ((uint32_t)val[2] << 8U) | val[3];
        }
        break;
    default:
        break;
    }
}

static void dhcp_parse_options(const uint8_t *data,
                               uint32_t length,
                               xNET_DHCP_Context_t *dhcp_ctx,
                               uint8_t *msg_type,
                               uint32_t *lease_time,
                               uint32_t *t1,
                               uint32_t *t2)
{
    uint32_t opt_idx = 240U;
    while (opt_idx < length)
    {
        uint8_t opt_code = data[opt_idx];
        if (opt_code == 255U)
        {
            break;
        }
        if (opt_code == 0U)
        {
            opt_idx++;
            continue;
        }
        if (opt_idx + 1U >= length)
        {
            break;
        }
        uint8_t opt_len = data[opt_idx + 1U];
        if (opt_idx + 2U + opt_len > length)
        {
            break;
        }

        const uint8_t *val = &data[opt_idx + 2U];
        dhcp_parse_network_option(opt_code, opt_len, val, dhcp_ctx);
        dhcp_parse_time_option(opt_code, opt_len, val, msg_type, lease_time, t1, t2);

        opt_idx += 2U + opt_len;
    }
}

static void
dhcp_handle_selecting_state(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx, uint8_t msg_type, const uint8_t *data)
{
    if (msg_type == 2U) // DHCPOFFER
    {
        // Offered IP address in header 'yiaddr' field
        (void)memcpy(dhcp_ctx->offered_ip.addr, &data[16], 4U);
        dhcp_ctx->state = xNET_DHCP_STATE_REQUESTING;
        dhcp_ctx->retry_elapsed_ms = 0U;
        dhcp_ctx->current_retry_timeout = 4000U;
        dhcp_ctx->retry_count = 0U;

        (void)dhcp_send_message(interface_ctx, 3U); // Send DHCPREQUEST
    }
}

static void dhcp_handle_active_state(xNET_Interface_Context_t *interface_ctx,
                                     xNET_DHCP_Context_t *dhcp_ctx,
                                     uint8_t msg_type,
                                     const uint8_t *data,
                                     uint32_t lease_time,
                                     uint32_t t1,
                                     uint32_t t2)
{
    if (msg_type == 5U) // DHCPACK
    {
        // Apply leased IP configuration
        (void)memcpy(interface_ctx->ip_addr.addr, &data[16], 4U);
        (void)memcpy(interface_ctx->netmask.addr, dhcp_ctx->netmask.addr, 4U);
        (void)memcpy(interface_ctx->gateway.addr, dhcp_ctx->gateway.addr, 4U);
        (void)memcpy(interface_ctx->dns_primary.addr, dhcp_ctx->dns_primary.addr, 4U);
        (void)memcpy(interface_ctx->dns_secondary.addr, dhcp_ctx->dns_secondary.addr, 4U);

        // Re-initialize ARP Cache (flushes old mappings)
        xNET_ARP_Cache_Init(interface_ctx);

        // Configure lease timings
        dhcp_ctx->lease_time = (lease_time != 0U) ? lease_time : 3600U;
        dhcp_ctx->t1 = (t1 != 0U) ? t1 : (dhcp_ctx->lease_time / 2U);
        dhcp_ctx->t2 = (t2 != 0U) ? t2 : ((dhcp_ctx->lease_time * 7U) / 8U);

        dhcp_ctx->state = xNET_DHCP_STATE_BOUND;
        dhcp_ctx->lease_elapsed_total_ms = 0U;
        dhcp_ctx->t1_elapsed_ms = 0U;
        dhcp_ctx->t2_elapsed_ms = 0U;
    }
    else if (msg_type == 6U) // DHCPNAK
    {
        // Clear current configuration
        (void)memset(interface_ctx->ip_addr.addr, 0, 4U);
        (void)memset(interface_ctx->netmask.addr, 0, 4U);
        (void)memset(interface_ctx->gateway.addr, 0, 4U);
        xNET_ARP_Cache_Init(interface_ctx);

        dhcp_ctx->state = xNET_DHCP_STATE_SELECTING;
        dhcp_ctx->xid++;
        dhcp_ctx->retry_elapsed_ms = 0U;
        dhcp_ctx->current_retry_timeout = 4000U;
        dhcp_ctx->retry_count = 0U;

        (void)dhcp_send_message(interface_ctx, 1U); // Restart DISCOVER
    }
    else
    {
        // Do nothing
    }
}

static bool dhcp_validate_rx_packet(const xNET_Interface_Context_t *interface_ctx,
                                    const xNET_DHCP_Context_t *dhcp_ctx,
                                    const uint8_t *data,
                                    uint32_t length)
{
    if (length < 240U)
    {
        return false;
    }

    if (data[0] != xNET_DHCP_OP_BOOTREPLY)
    {
        return false;
    }

    uint32_t xid_be;
    (void)memcpy(&xid_be, &data[4], 4U);
    if (xNET_NTOHL(xid_be) != dhcp_ctx->xid)
    {
        return false;
    }

    if (memcmp(&data[28], interface_ctx->mac_addr.addr, 6U) != 0)
    {
        return false;
    }

    uint32_t cookie_be;
    (void)memcpy(&cookie_be, &data[236], 4U);
    if (xNET_NTOHL(cookie_be) != xNET_DHCP_MAGIC_COOKIE)
    {
        return false;
    }

    return true;
}

static xRETURN_t dhcp_rx_callback(xNET_UDP_Context_t *udp_ctx,
                                  const xNET_IPv4_Address_t *remote_addr,
                                  uint16_t remote_port,
                                  const uint8_t *data,
                                  uint32_t length)
{
    (void)remote_addr;
    (void)remote_port;

    // Resolve interface context from embedded socket context
    xNET_Interface_Context_t *interface_ctx =
        (xNET_Interface_Context_t *)((uint8_t *)udp_ctx - offsetof(xNET_Interface_Context_t, dhcp_ctx.udp_socket));
    xNET_DHCP_Context_t *dhcp_ctx = &interface_ctx->dhcp_ctx;

    if (!dhcp_ctx->is_active)
    {
        return xRETURN_xNET_OK;
    }

    if (!dhcp_validate_rx_packet(interface_ctx, dhcp_ctx, data, length))
    {
        return xRETURN_xNET_OK;
    }

    // Parse options
    uint8_t msg_type = 0U;
    uint32_t lease_time = 0U;
    uint32_t t1 = 0U;
    uint32_t t2 = 0U;
    dhcp_parse_options(data, length, dhcp_ctx, &msg_type, &lease_time, &t1, &t2);

    // Process State transitions
    if (dhcp_ctx->state == xNET_DHCP_STATE_SELECTING)
    {
        dhcp_handle_selecting_state(interface_ctx, dhcp_ctx, msg_type, data);
    }
    else if ((dhcp_ctx->state == xNET_DHCP_STATE_REQUESTING) || (dhcp_ctx->state == xNET_DHCP_STATE_RENEWING) ||
             (dhcp_ctx->state == xNET_DHCP_STATE_REBINDING))
    {
        dhcp_handle_active_state(interface_ctx, dhcp_ctx, msg_type, data, lease_time, t1, t2);
    }
    else
    {
        // Ignore other states
    }

    return xRETURN_xNET_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xNET_DHCP_Start(xNET_Interface_Context_t *interface_ctx)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if (interface_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xNET_DHCP_Context_t *dhcp_ctx = &interface_ctx->dhcp_ctx;

    // Reset context
    (void)memset(dhcp_ctx, 0, sizeof(xNET_DHCP_Context_t));

    dhcp_ctx->state = xNET_DHCP_STATE_INIT;
    dhcp_ctx->is_active = true;
    dhcp_ctx->xid = 0x3F82A000U + interface_ctx->net_ctx->system_ticks;
    dhcp_ctx->current_retry_timeout = 4000U;
    dhcp_ctx->retry_elapsed_ms = 0U;

    // Open UDP Socket on port 68
    xRETURN_t ret = xNET_UDP_Open(interface_ctx->net_ctx, &dhcp_ctx->udp_socket, xNET_DHCP_CLIENT_PORT, dhcp_rx_callback);
    if (ret != xRETURN_xNET_OK)
    {
        dhcp_ctx->is_active = false;
        return ret;
    }

    // Clear IP configuration initially
    (void)memset(interface_ctx->ip_addr.addr, 0, 4);
    (void)memset(interface_ctx->netmask.addr, 0, 4);
    (void)memset(interface_ctx->gateway.addr, 0, 4);
    xNET_ARP_Cache_Init(interface_ctx);

    // Send DHCPDISCOVER
    dhcp_ctx->state = xNET_DHCP_STATE_SELECTING;
    return dhcp_send_message(interface_ctx, 1U);
}

xRETURN_t xNET_DHCP_Stop(xNET_Interface_Context_t *interface_ctx)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if (interface_ctx == NULL)
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    xNET_DHCP_Context_t *dhcp_ctx = &interface_ctx->dhcp_ctx;
    if (dhcp_ctx->is_active)
    {
        dhcp_ctx->is_active = false;
        dhcp_ctx->state = xNET_DHCP_STATE_FAILED;
        (void)xNET_UDP_Close(&dhcp_ctx->udp_socket);
    }

    return xRETURN_xNET_OK;
}

void xNET_DHCP_Tick(xNET_Interface_Context_t *interface_ctx, uint32_t elapsed_ms)
{
    xASSERT(interface_ctx != NULL, "interface_ctx is NULL");
    if (interface_ctx == NULL)
    {
        return;
    }

    xNET_DHCP_Context_t *dhcp_ctx = &interface_ctx->dhcp_ctx;
    if (!dhcp_ctx->is_active)
    {
        return;
    }

    dhcp_tick_retries(interface_ctx, dhcp_ctx, elapsed_ms);
    dhcp_tick_lease(interface_ctx, dhcp_ctx, elapsed_ms);
}

static void dhcp_tick_retries(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx, uint32_t elapsed_ms)
{
    if ((dhcp_ctx->state == xNET_DHCP_STATE_SELECTING) || (dhcp_ctx->state == xNET_DHCP_STATE_REQUESTING) ||
        (dhcp_ctx->state == xNET_DHCP_STATE_RENEWING) || (dhcp_ctx->state == xNET_DHCP_STATE_REBINDING))
    {
        dhcp_ctx->retry_elapsed_ms += elapsed_ms;
        if (dhcp_ctx->retry_elapsed_ms >= dhcp_ctx->current_retry_timeout)
        {
            dhcp_ctx->retry_elapsed_ms = 0U;
            if (dhcp_ctx->retry_count < 4U)
            {
                dhcp_ctx->retry_count++;
                dhcp_ctx->current_retry_timeout *= 2U;

                uint8_t msg_type = 3U;
                if (dhcp_ctx->state == xNET_DHCP_STATE_SELECTING)
                {
                    msg_type = 1U;
                }
                (void)dhcp_send_message(interface_ctx, msg_type);
            }
            else
            {
                dhcp_handle_retry_exhausted(interface_ctx, dhcp_ctx);
            }
        }
    }
}

static void dhcp_handle_retry_exhausted(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx)
{
    if (dhcp_ctx->state == xNET_DHCP_STATE_SELECTING)
    {
        dhcp_ctx->state = xNET_DHCP_STATE_FAILED;
        (void)xNET_DHCP_Stop(interface_ctx);
    }
    else if (dhcp_ctx->state == xNET_DHCP_STATE_REQUESTING)
    {
        dhcp_ctx->xid++;
        dhcp_ctx->retry_count = 0U;
        dhcp_ctx->current_retry_timeout = 4000U;
        dhcp_ctx->state = xNET_DHCP_STATE_SELECTING;
        (void)dhcp_send_message(interface_ctx, 1U);
    }
    else if (dhcp_ctx->state == xNET_DHCP_STATE_RENEWING)
    {
        dhcp_ctx->state = xNET_DHCP_STATE_REBINDING;
        dhcp_ctx->retry_count = 0U;
        dhcp_ctx->current_retry_timeout = 4000U;
        (void)dhcp_send_message(interface_ctx, 3U);
    }
    else // REBINDING
    {
        dhcp_ctx->state = xNET_DHCP_STATE_INIT;
        dhcp_ctx->xid++;
        dhcp_ctx->retry_count = 0U;
        dhcp_ctx->current_retry_timeout = 4000U;

        // Clear IP configuration
        (void)memset(interface_ctx->ip_addr.addr, 0, 4);
        (void)memset(interface_ctx->netmask.addr, 0, 4);
        (void)memset(interface_ctx->gateway.addr, 0, 4);
        xNET_ARP_Cache_Init(interface_ctx);

        dhcp_ctx->state = xNET_DHCP_STATE_SELECTING;
        (void)dhcp_send_message(interface_ctx, 1U);
    }
}

static void dhcp_tick_lease(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx, uint32_t elapsed_ms)
{
    if ((dhcp_ctx->state == xNET_DHCP_STATE_BOUND) || (dhcp_ctx->state == xNET_DHCP_STATE_RENEWING) ||
        (dhcp_ctx->state == xNET_DHCP_STATE_REBINDING))
    {
        dhcp_ctx->t1_elapsed_ms += elapsed_ms;
        dhcp_ctx->t2_elapsed_ms += elapsed_ms;
        dhcp_ctx->lease_elapsed_total_ms += elapsed_ms;

        // check lease expiration (100%)
        if (dhcp_ctx->lease_elapsed_total_ms >= (dhcp_ctx->lease_time * 1000U))
        {
            dhcp_handle_lease_expired(interface_ctx, dhcp_ctx);
        }
        else if (dhcp_ctx->state == xNET_DHCP_STATE_BOUND)
        {
            // Check T1 expiration (50%)
            if (dhcp_ctx->t1_elapsed_ms >= (dhcp_ctx->t1 * 1000U))
            {
                dhcp_ctx->state = xNET_DHCP_STATE_RENEWING;
                dhcp_ctx->retry_elapsed_ms = 0U;
                dhcp_ctx->current_retry_timeout = 4000U;
                dhcp_ctx->retry_count = 0U;
                (void)dhcp_send_message(interface_ctx, 3U);
            }
        }
        else if (dhcp_ctx->state == xNET_DHCP_STATE_RENEWING)
        {
            // Check T2 expiration (87.5%)
            if (dhcp_ctx->t2_elapsed_ms >= (dhcp_ctx->t2 * 1000U))
            {
                dhcp_ctx->state = xNET_DHCP_STATE_REBINDING;
                dhcp_ctx->retry_elapsed_ms = 0U;
                dhcp_ctx->current_retry_timeout = 4000U;
                dhcp_ctx->retry_count = 0U;
                (void)dhcp_send_message(interface_ctx, 3U);
            }
        }
        else
        {
            // REBINDING, wait for ACK or lease expiration
        }
    }
}

static void dhcp_handle_lease_expired(xNET_Interface_Context_t *interface_ctx, xNET_DHCP_Context_t *dhcp_ctx)
{
    // Clear current configuration
    (void)memset(interface_ctx->ip_addr.addr, 0, 4);
    (void)memset(interface_ctx->netmask.addr, 0, 4);
    (void)memset(interface_ctx->gateway.addr, 0, 4);
    xNET_ARP_Cache_Init(interface_ctx);

    dhcp_ctx->xid++;
    dhcp_ctx->retry_count = 0U;
    dhcp_ctx->current_retry_timeout = 4000U;
    dhcp_ctx->retry_elapsed_ms = 0U;
    dhcp_ctx->state = xNET_DHCP_STATE_SELECTING;
    (void)dhcp_send_message(interface_ctx, 1U);
}

// EOF /////////////////////////////////////////////////////////////////////////////
