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

// @file xnet_dns.c
// @brief DNS client implementation.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xnet_dns.h"
#include "xnet_core.h"
#include "xnet_config.h"
#include "xnet_interface.h"
#include "xnet_udp.h"
#include "xassert.h"

#include "xnet_log.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
static uint32_t dns_encode_name(const char *hostname, uint8_t *dest);
static bool dns_encode_step(const char *hostname, uint32_t *src_idx, uint8_t *dest, uint32_t *dest_idx);

static xRETURN_t dns_send_query(xNET_DNS_Context_t *dns_ctx);

static uint32_t dns_skip_name(const uint8_t *data, uint32_t length, uint32_t idx);

static bool dns_validate_rx_packet(const xNET_DNS_Context_t *dns_ctx, const uint8_t *data, uint32_t length);

static void dns_handle_query_failed(xNET_DNS_Context_t *dns_ctx, xRETURN_t status);

static uint32_t dns_parse_questions(const uint8_t *data, uint32_t length, uint16_t questions, uint32_t idx);

static bool dns_parse_answers(const uint8_t *data, uint32_t length, uint16_t answers, uint32_t idx, xNET_IPv4_Address_t *resolved_ip);

static bool dns_find_server(const xNET_Context_t *net_ctx, xNET_IPv4_Address_t *dns_server);

static void dns_copy_hostname(char *dest, const char *src);

static xRETURN_t dns_rx_callback(xNET_UDP_Context_t *udp_ctx,
                                 const xNET_IPv4_Address_t *remote_addr,
                                 uint16_t remote_port,
                                 const uint8_t *data,
                                 uint32_t length);

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

static bool dns_encode_step(const char *hostname, uint32_t *src_idx, uint8_t *dest, uint32_t *dest_idx)
{
    uint32_t s_idx = *src_idx;
    uint32_t label_len = 0U;
    while ((hostname[s_idx + label_len] != '\0') && (hostname[s_idx + label_len] != '.'))
    {
        label_len++;
    }

    if ((label_len == 0U) || (label_len > 63U))
    {
        return false;
    }

    uint32_t d_idx = *dest_idx;
    dest[d_idx] = (uint8_t)label_len;
    d_idx++;

    for (uint32_t i = 0U; i < label_len; i++)
    {
        dest[d_idx] = (uint8_t)hostname[s_idx + i];
        d_idx++;
    }

    s_idx += label_len;
    if (hostname[s_idx] == '.')
    {
        s_idx++;
    }

    *src_idx = s_idx;
    *dest_idx = d_idx;
    return true;
}

static uint32_t dns_encode_name(const char *hostname, uint8_t *dest)
{
    if ((hostname == NULL) || (dest == NULL) || (hostname[0] == '\0'))
    {
        return 0U;
    }

    uint32_t src_idx = 0U;
    uint32_t dest_idx = 0U;

    while (hostname[src_idx] != '\0')
    {
        if (!dns_encode_step(hostname, &src_idx, dest, &dest_idx))
        {
            return 0U;
        }
    }

    dest[dest_idx] = 0U;
    dest_idx++;

    return dest_idx;
}

static xRETURN_t dns_send_query(xNET_DNS_Context_t *dns_ctx)
{
    uint8_t payload[512];
    (void)memset(payload, 0, sizeof(payload));

    // Header
    uint16_t id_be = xNET_HTONS(dns_ctx->transaction_id);
    (void)memcpy(&payload[0], &id_be, 2);

    payload[2] = 0x01U; // Flags: RD = 1
    payload[3] = 0x00U;

    payload[4] = 0x00U; // Questions = 1
    payload[5] = 0x01U;

    // Encode hostname starting at payload[12]
    uint32_t name_len = dns_encode_name(dns_ctx->hostname, &payload[12]);
    if (name_len == 0U)
    {
        return xRETURN_xERR_xNET_INVALID_ARGUMENT;
    }

    uint32_t idx = 12U + name_len;

    // Type A: 0x0001
    payload[idx] = 0x00U;
    payload[idx + 1U] = 0x01U;

    // Class IN: 0x0001
    payload[idx + 2U] = 0x00U;
    payload[idx + 3U] = 0x01U;

    uint32_t total_len = idx + 4U;

    return xNET_UDP_Send_To(&dns_ctx->udp_socket, &dns_ctx->dns_server, 53U, payload, total_len);
}

static uint32_t dns_skip_name(const uint8_t *data, uint32_t length, uint32_t idx)
{
    uint32_t curr = idx;
    uint32_t visited = 0U;

    while (curr < length)
    {
        uint8_t len = data[curr];
        if (len == 0U)
        {
            return curr + 1U;
        }

        if ((len & 0xC0U) == 0xC0U)
        {
            if (curr + 2U > length)
            {
                return 0U;
            }
            return curr + 2U;
        }

        curr += 1U + len;

        if (curr > length)
        {
            return 0U;
        }

        visited++;
        if (visited > 128U)
        {
            return 0U;
        }
    }

    return 0U;
}

static bool dns_validate_rx_packet(const xNET_DNS_Context_t *dns_ctx, const uint8_t *data, uint32_t length)
{
    if (length < 12U)
    {
        return false;
    }

    uint16_t id_be;
    (void)memcpy(&id_be, &data[0], 2);
    if (xNET_NTOHS(id_be) != dns_ctx->transaction_id)
    {
        return false;
    }

    uint8_t qr = (data[2] & 0x80U);
    if (qr == 0U)
    {
        return false;
    }

    return true;
}

static void dns_handle_query_failed(xNET_DNS_Context_t *dns_ctx, xRETURN_t status)
{
    dns_ctx->is_active = false;
    dns_ctx->state = xNET_DNS_STATE_FAILED;
    dns_ctx->result_status = status;
    (void)xNET_UDP_Close(&dns_ctx->udp_socket);

    if (dns_ctx->on_result != NULL)
    {
        (void)dns_ctx->on_result(dns_ctx, dns_ctx->hostname, NULL, status);
    }
}

static uint32_t dns_parse_questions(const uint8_t *data, uint32_t length, uint16_t questions, uint32_t idx)
{
    uint32_t curr_idx = idx;
    for (uint16_t q = 0U; q < questions; q++)
    {
        curr_idx = dns_skip_name(data, length, curr_idx);
        if (curr_idx == 0U)
        {
            return 0U;
        }

        if (curr_idx + 4U > length)
        {
            return 0U;
        }
        curr_idx += 4U;
    }
    return curr_idx;
}

static bool dns_parse_answers(const uint8_t *data, uint32_t length, uint16_t answers, uint32_t idx, xNET_IPv4_Address_t *resolved_ip)
{
    uint32_t curr_idx = idx;
    for (uint16_t a = 0U; a < answers; a++)
    {
        curr_idx = dns_skip_name(data, length, curr_idx);
        if (curr_idx == 0U)
        {
            return false;
        }

        if (curr_idx + 10U > length)
        {
            return false;
        }

        uint16_t type_be;
        uint16_t class_be;
        uint16_t rdlength_be;

        (void)memcpy(&type_be, &data[curr_idx], 2);
        (void)memcpy(&class_be, &data[curr_idx + 2U], 2);
        (void)memcpy(&rdlength_be, &data[curr_idx + 8U], 2);

        uint16_t type = xNET_NTOHS(type_be);
        uint16_t class = xNET_NTOHS(class_be);
        uint16_t rdlength = xNET_NTOHS(rdlength_be);

        curr_idx += 10U;
        if (curr_idx + rdlength > length)
        {
            return false;
        }

        if ((type == 1U) && (class == 1U) && (rdlength == 4U))
        {
            (void)memcpy(resolved_ip->addr, &data[curr_idx], 4);
            return true;
        }

        curr_idx += rdlength;
    }
    return false;
}

static bool dns_find_server(const xNET_Context_t *net_ctx, xNET_IPv4_Address_t *dns_server)
{
    const xNET_Interface_Context_t *interface_ctx = net_ctx->interface_list;
    while (interface_ctx != NULL)
    {
        bool ip_nonzero = (interface_ctx->ip_addr.addr[0] != 0U) || (interface_ctx->ip_addr.addr[1] != 0U) ||
                          (interface_ctx->ip_addr.addr[2] != 0U) || (interface_ctx->ip_addr.addr[3] != 0U);
        bool dns_nonzero = (interface_ctx->dns_primary.addr[0] != 0U) || (interface_ctx->dns_primary.addr[1] != 0U) ||
                           (interface_ctx->dns_primary.addr[2] != 0U) || (interface_ctx->dns_primary.addr[3] != 0U);

        if (ip_nonzero && dns_nonzero)
        {
            *dns_server = interface_ctx->dns_primary;
            return true;
        }
        interface_ctx = interface_ctx->next;
    }
    return false;
}

static void dns_copy_hostname(char *dest, const char *src)
{
    uint32_t len = 0U;
    while ((len < 255U) && (src[len] != '\0'))
    {
        dest[len] = src[len];
        len++;
    }
    dest[len] = '\0';
}

static xRETURN_t dns_rx_callback(xNET_UDP_Context_t *udp_ctx,
                                 const xNET_IPv4_Address_t *remote_addr,
                                 uint16_t remote_port,
                                 const uint8_t *data,
                                 uint32_t length)
{
    (void)remote_addr;
    (void)remote_port;

    xNET_DNS_Context_t *dns_ctx = (xNET_DNS_Context_t *)((uint8_t *)udp_ctx - offsetof(xNET_DNS_Context_t, udp_socket));

    if (!dns_ctx->is_active)
    {
        return xRETURN_xNET_OK;
    }

    if (!dns_validate_rx_packet(dns_ctx, data, length))
    {
        return xRETURN_xNET_OK;
    }

    // RCODE is in the low 4 bits of the 4th byte (data[3]).
    // Byte 2 (data[2]): QR (1 bit), Opcode (4 bits), AA (1 bit), TC (1 bit), RD (1 bit)
    // Byte 3 (data[3]): RA (1 bit), Z (3 bits), RCODE (4 bits)
    uint8_t rcode = (data[3] & 0x0FU);

    if (rcode != 0U)
    {
        xRETURN_t err_status = (rcode == 3U) ? xRETURN_xERR_xNET_DNS_NAME_ERROR : xRETURN_xERR_xNET_DNS_SERVER_ERROR;
        dns_handle_query_failed(dns_ctx, err_status);
        return xRETURN_xNET_OK;
    }

    uint16_t questions_be;
    uint16_t answers_be;
    (void)memcpy(&questions_be, &data[4], 2);
    (void)memcpy(&answers_be, &data[6], 2);
    uint16_t questions = xNET_NTOHS(questions_be);
    uint16_t answers = xNET_NTOHS(answers_be);

    if (questions == 0U)
    {
        return xRETURN_xNET_OK;
    }

    uint32_t idx = dns_parse_questions(data, length, questions, 12U);
    if (idx == 0U)
    {
        return xRETURN_xNET_OK;
    }

    xNET_IPv4_Address_t resolved_ip = {{0, 0, 0, 0}};
    if (dns_parse_answers(data, length, answers, idx, &resolved_ip))
    {
        dns_ctx->is_active = false;
        dns_ctx->state = xNET_DNS_STATE_SUCCESS;
        dns_ctx->resolved_ip = resolved_ip;
        dns_ctx->result_status = xRETURN_xNET_OK;
        (void)xNET_UDP_Close(&dns_ctx->udp_socket);

        if (dns_ctx->on_result != NULL)
        {
            (void)dns_ctx->on_result(dns_ctx, dns_ctx->hostname, &resolved_ip, xRETURN_xNET_OK);
        }
    }
    else
    {
        dns_handle_query_failed(dns_ctx, xRETURN_xERR_xNET_NOT_FOUND);
    }

    return xRETURN_xNET_OK;
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

xRETURN_t xNET_DNS_Query_A(xNET_Context_t *net_ctx, xNET_DNS_Context_t *dns_ctx, const char *hostname, xNET_DNS_Result_Callback_t on_result)
{
    xASSERT(net_ctx != NULL, "net_ctx is NULL");
    xASSERT(dns_ctx != NULL, "dns_ctx is NULL");
    xASSERT(hostname != NULL, "hostname is NULL");
    xASSERT(on_result != NULL, "on_result is NULL");

    if ((net_ctx == NULL) || (dns_ctx == NULL) || (hostname == NULL) || (on_result == NULL))
    {
        return xRETURN_xERR_xNET_NULL_POINTER;
    }

    if (dns_ctx->is_active)
    {
        return xRETURN_xERR_xNET_DNS_BUSY;
    }

    xNET_IPv4_Address_t dns_server = {{0, 0, 0, 0}};
    if (!dns_find_server(net_ctx, &dns_server))
    {
        return xRETURN_xERR_xNET_DNS_NO_SERVER;
    }

    (void)memset(dns_ctx, 0, sizeof(xNET_DNS_Context_t));
    dns_ctx->net_ctx = net_ctx;
    dns_ctx->state = xNET_DNS_STATE_QUERYING;
    dns_ctx->is_active = true;
    dns_ctx->on_result = on_result;
    dns_ctx->dns_server = dns_server;
    dns_ctx->current_retry_timeout = xNET_CONFIG_DNS_RETRY_TIMEOUT_MS;
    dns_ctx->retry_count = 0U;
    dns_ctx->retry_elapsed_ms = 0U;
    dns_ctx->transaction_id = (uint16_t)(0x1A2B5C00U + net_ctx->system_ticks);

    dns_copy_hostname(dns_ctx->hostname, hostname);

    xRETURN_t ret = xNET_UDP_Open(net_ctx, &dns_ctx->udp_socket, 0U, dns_rx_callback);
    if (ret != xRETURN_xNET_OK)
    {
        dns_ctx->is_active = false;
        dns_ctx->state = xNET_DNS_STATE_FAILED;
        return ret;
    }

    ret = dns_send_query(dns_ctx);
    if (ret != xRETURN_xNET_OK)
    {
        dns_ctx->is_active = false;
        dns_ctx->state = xNET_DNS_STATE_FAILED;
        (void)xNET_UDP_Close(&dns_ctx->udp_socket);
        return ret;
    }

    return xRETURN_xNET_OK;
}

void xNET_DNS_Tick(xNET_DNS_Context_t *dns_ctx, uint32_t elapsed_ms)
{
    xASSERT(dns_ctx != NULL, "dns_ctx is NULL");
    if (dns_ctx == NULL)
    {
        return;
    }

    if (!dns_ctx->is_active)
    {
        return;
    }

    dns_ctx->retry_elapsed_ms += elapsed_ms;
    if (dns_ctx->retry_elapsed_ms >= dns_ctx->current_retry_timeout)
    {
        dns_ctx->retry_elapsed_ms = 0U;
        if (dns_ctx->retry_count < xNET_CONFIG_DNS_MAX_RETRIES)
        {
            dns_ctx->retry_count++;
            dns_ctx->current_retry_timeout *= 2U;

            (void)dns_send_query(dns_ctx);
        }
        else
        {
            dns_handle_query_failed(dns_ctx, xRETURN_xERR_xNET_TIMEOUT);
        }
    }
}

// EOF /////////////////////////////////////////////////////////////////////////////
