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

// @file test_xnet_dns.c
// @brief Host tests for the xNET DNS Client.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// MODULE INCLUDES
#include "unity.h"
#include "xnet_core.h"
#include "xnet_defs.h"
#include "xnet_config.h"
#include "xnet_return.h"
#include "xnet_interface_fake.h"
#include "xnet_ethernet.h"
#include "xnet_ipv4.h"
#include "xnet_udp.h"
#include "xnet_dns.h"

// MACROS //////////////////////////////////////////////////////////////////////////

// Overriding weak xassert_system_halt to prevent hanging on assertions in unit tests
void xassert_system_halt(void)
{
    // Do nothing: allows the test to continue and verify the return value
}

// MOCKS & STATE ///////////////////////////////////////////////////////////////////
static xNET_Context_t g_net_ctx;
static xNET_Interface_Context_t g_interface_ctx;
static xNET_Fake_Interface_Context_t g_fake_driver_ctx;
static uint8_t g_packet_pool_buffer[16384];

static uint32_t s_callback_count = 0;
static char s_last_hostname[256];
static xNET_IPv4_Address_t s_last_resolved_ip;
static xRETURN_t s_last_result = 0;

static xRETURN_t mock_dns_callback(xNET_DNS_Context_t *dns_ctx, const char *hostname, const xNET_IPv4_Address_t *ip_addr, xRETURN_t result)
{
    (void)dns_ctx;
    s_callback_count++;
    uint32_t len = 0U;
    while ((len < 255U) && (hostname[len] != '\0'))
    {
        s_last_hostname[len] = hostname[len];
        len++;
    }
    s_last_hostname[len] = '\0';
    s_last_result = result;
    if (ip_addr != NULL)
    {
        s_last_resolved_ip = *ip_addr;
    }
    else
    {
        (void)memset(&s_last_resolved_ip, 0, sizeof(s_last_resolved_ip));
    }
    return xRETURN_xNET_OK;
}

// HELPER FUNCTIONS ////////////////////////////////////////////////////////////////
void setUp(void)
{
    s_callback_count = 0;
    (void)memset(s_last_hostname, 0, sizeof(s_last_hostname));
    (void)memset(&s_last_resolved_ip, 0, sizeof(s_last_resolved_ip));
    s_last_result = 0;

    // Initialize core net context
    xNET_Config_t net_config;
    (void)memset(&net_config, 0, sizeof(net_config));
    net_config.packet_pool_buffer = g_packet_pool_buffer;
    net_config.packet_pool_buffer_size = sizeof(g_packet_pool_buffer);
    (void)xNET_Init(&g_net_ctx, &net_config);

    // Initialize fake interface
    (void)memset(&g_interface_ctx, 0, sizeof(g_interface_ctx));
    g_interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x02, 0x00, 0x00, 0x00, 0x00, 0x01}};
    g_interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    g_interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    g_interface_ctx.gateway = (xNET_IPv4_Address_t){{192, 168, 1, 1}};
    g_interface_ctx.dns_primary = (xNET_IPv4_Address_t){{8, 8, 8, 8}};

    (void)xNET_Fake_Interface_Init(&g_fake_driver_ctx, &g_interface_ctx);
    (void)xNET_Interface_Add(&g_net_ctx, &g_interface_ctx);
    (void)xNET_Interface_Link_Set(&g_interface_ctx, true);

    // Pre-populate the gateway in the ARP cache to prevent unresolved ARP request crashes
    g_interface_ctx.arp_cache[0].ip_addr = g_interface_ctx.gateway;
    g_interface_ctx.arp_cache[0].mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    g_interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;
    g_interface_ctx.arp_cache[0].timeout_ms = 0xFFFFFFFFU;

    // Enable hardware checksum offload capabilities to ignore manual checksum calculations
    g_interface_ctx.checksum_caps = xNET_CHECKSUM_CAP_IP_RX | xNET_CHECKSUM_CAP_UDP_RX;
}

void tearDown(void)
{
}

// TEST CASES //////////////////////////////////////////////////////////////////////

void test_xnet_dns_no_server(void)
{
    xNET_DNS_Context_t dns_ctx;
    (void)memset(&dns_ctx, 0, sizeof(dns_ctx));

    // Clear primary DNS server to trigger DNS NO SERVER error
    g_interface_ctx.dns_primary = (xNET_IPv4_Address_t){{0, 0, 0, 0}};

    xRETURN_t ret = xNET_DNS_Query_A(&g_net_ctx, &dns_ctx, "test.local", mock_dns_callback);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_DNS_NO_SERVER, ret);
}

void test_xnet_dns_busy(void)
{
    xNET_DNS_Context_t dns_ctx;
    (void)memset(&dns_ctx, 0, sizeof(dns_ctx));

    xRETURN_t ret = xNET_DNS_Query_A(&g_net_ctx, &dns_ctx, "test.local", mock_dns_callback);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, ret);

    // Call query again on the same active context
    ret = xNET_DNS_Query_A(&g_net_ctx, &dns_ctx, "test.local", mock_dns_callback);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_DNS_BUSY, ret);

    // Clean up
    dns_ctx.is_active = false;
    (void)xNET_UDP_Close(&dns_ctx.udp_socket);
}

void test_xnet_dns_query_success(void)
{
    xNET_DNS_Context_t dns_ctx;
    (void)memset(&dns_ctx, 0, sizeof(dns_ctx));

    xRETURN_t ret = xNET_DNS_Query_A(&g_net_ctx, &dns_ctx, "google.com", mock_dns_callback);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, ret);
    TEST_ASSERT_TRUE(dns_ctx.is_active);
    TEST_ASSERT_EQUAL(xNET_DNS_STATE_QUERYING, dns_ctx.state);

    // Process to flush TX
    (void)xNET_Process(&g_net_ctx);

    // Check TX queue to verify the DNS query packet
    uint8_t tx_frame[1000];
    uint32_t tx_len = 0;
    xRETURN_t tx_ret = xNET_Fake_Interface_TX_Pop(&g_fake_driver_ctx, tx_frame, sizeof(tx_frame), &tx_len);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, tx_ret);
    TEST_ASSERT_TRUE(tx_len > 42U); // Ethernet + IP + UDP + DNS Query

    // Extract UDP payload (IPv4 header offset 14, starts at 34, length starts at 38)
    uint16_t udp_len_be;
    memcpy(&udp_len_be, &tx_frame[38], 2);
    uint16_t udp_len = xNET_NTOHS(udp_len_be);
    uint16_t dns_payload_len = udp_len - 8;

    const uint8_t *dns_payload = &tx_frame[42];

    // Verify transaction ID matches
    uint16_t tx_xid_be;
    memcpy(&tx_xid_be, &dns_payload[0], 2);
    TEST_ASSERT_EQUAL_UINT16(dns_ctx.transaction_id, xNET_NTOHS(tx_xid_be));

    // Construct valid DNS A reply packet matching query details
    uint8_t rx_dns[128];
    (void)memset(rx_dns, 0, sizeof(rx_dns));

    // Transaction ID
    memcpy(&rx_dns[0], &dns_payload[0], 2);
    // Flags: Response, standard query, recursion desired, recursion available, no error
    rx_dns[2] = 0x81;
    rx_dns[3] = 0x80;
    // Questions = 1
    rx_dns[4] = 0;
    rx_dns[5] = 1;
    // Answers = 1
    rx_dns[6] = 0;
    rx_dns[7] = 1;

    // Copy Question section from query (Name + Type + Class)
    uint32_t name_and_q_len = dns_payload_len - 12;
    memcpy(&rx_dns[12], &dns_payload[12], name_and_q_len);

    uint32_t idx = 12 + name_and_q_len;

    // Answer RR:
    // Name pointer: 0xC00C (refers to hostname offset 12 in DNS payload)
    rx_dns[idx] = 0xC0;
    rx_dns[idx + 1] = 0x0C;
    // Type A: 0x0001
    rx_dns[idx + 2] = 0x00;
    rx_dns[idx + 3] = 0x01;
    // Class IN: 0x0001
    rx_dns[idx + 4] = 0x00;
    rx_dns[idx + 5] = 0x01;
    // TTL: 300 seconds (0x0000012C)
    rx_dns[idx + 6] = 0x00;
    rx_dns[idx + 7] = 0x00;
    rx_dns[idx + 8] = 0x01;
    rx_dns[idx + 9] = 0x2C;
    // RDLENGTH: 4 (IPv4 address)
    rx_dns[idx + 10] = 0x00;
    rx_dns[idx + 11] = 0x04;
    // RDATA: 142.250.190.46
    rx_dns[idx + 12] = 142;
    rx_dns[idx + 13] = 250;
    rx_dns[idx + 14] = 190;
    rx_dns[idx + 15] = 46;

    uint32_t rx_dns_len = idx + 16;

    // Build raw Ethernet + IPv4 + UDP packet to inject
    uint8_t rx_frame[256];
    // Ethernet header
    memcpy(&rx_frame[0], g_interface_ctx.mac_addr.addr, 6);
    // Source MAC: DNS router/server
    memset(&rx_frame[6], 0x11, 6);
    rx_frame[12] = 0x08;
    rx_frame[13] = 0x00; // IPv4

    // IPv4 Header (20 bytes)
    rx_frame[14] = 0x45;
    rx_frame[15] = 0x00;
    uint16_t ip_total_len = 20 + 8 + rx_dns_len;
    rx_frame[16] = (uint8_t)((ip_total_len >> 8) & 0xFFU);
    rx_frame[17] = (uint8_t)(ip_total_len & 0xFFU);
    // Ident, Flags/Offset
    memset(&rx_frame[18], 0, 4);
    rx_frame[22] = 64; // TTL
    rx_frame[23] = 17; // UDP protocol
    rx_frame[24] = 0;  // Checksum (ignore/offload mock)
    rx_frame[25] = 0;
    // Src IP: 8.8.8.8
    memcpy(&rx_frame[26], g_interface_ctx.dns_primary.addr, 4);
    // Dest IP: 192.168.1.10
    memcpy(&rx_frame[30], g_interface_ctx.ip_addr.addr, 4);

    // UDP Header (8 bytes)
    // Src Port: 53
    rx_frame[34] = 0x00;
    rx_frame[35] = 0x35;
    // Dest Port: ephemeral port used by query
    uint16_t dest_port_be = xNET_HTONS(dns_ctx.udp_socket.local_port);
    memcpy(&rx_frame[36], &dest_port_be, 2);
    // Length
    uint16_t udp_total_len = 8 + rx_dns_len;
    rx_frame[38] = (uint8_t)((udp_total_len >> 8) & 0xFFU);
    rx_frame[39] = (uint8_t)(udp_total_len & 0xFFU);
    // Checksum
    rx_frame[40] = 0;
    rx_frame[41] = 0;

    // DNS Payload
    memcpy(&rx_frame[42], rx_dns, rx_dns_len);

    uint32_t rx_frame_len = 14 + ip_total_len;

    // Inject reply with hardware-checksum-validated flags
    xRETURN_t rx_ret =
        xNET_Interface_RX_Frame(&g_interface_ctx, rx_frame, rx_frame_len, xNET_RX_FLAG_IP_CHECKSUM_VALID | xNET_RX_FLAG_L4_CHECKSUM_VALID);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, rx_ret);

    // Poll to process RX
    (void)xNET_Process(&g_net_ctx);

    // Verify callback execution and data correctness
    TEST_ASSERT_EQUAL_UINT32(1U, s_callback_count);
    TEST_ASSERT_EQUAL_STRING("google.com", s_last_hostname);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, s_last_result);
    TEST_ASSERT_EQUAL_UINT8(142, s_last_resolved_ip.addr[0]);
    TEST_ASSERT_EQUAL_UINT8(250, s_last_resolved_ip.addr[1]);
    TEST_ASSERT_EQUAL_UINT8(190, s_last_resolved_ip.addr[2]);
    TEST_ASSERT_EQUAL_UINT8(46, s_last_resolved_ip.addr[3]);

    TEST_ASSERT_FALSE(dns_ctx.is_active);
    TEST_ASSERT_EQUAL(xNET_DNS_STATE_SUCCESS, dns_ctx.state);
}

void test_xnet_dns_query_nxdomain(void)
{
    xNET_DNS_Context_t dns_ctx;
    (void)memset(&dns_ctx, 0, sizeof(dns_ctx));

    xRETURN_t ret = xNET_DNS_Query_A(&g_net_ctx, &dns_ctx, "google.com", mock_dns_callback);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, ret);

    // Process to flush TX
    (void)xNET_Process(&g_net_ctx);

    // Intercept query to get transaction ID
    uint8_t tx_frame[1000];
    uint32_t tx_len = 0;
    (void)xNET_Fake_Interface_TX_Pop(&g_fake_driver_ctx, tx_frame, sizeof(tx_frame), &tx_len);

    const uint8_t *dns_payload = &tx_frame[42];

    // Construct NXDOMAIN (RCODE = 3) DNS reply
    uint8_t rx_dns[64];
    (void)memset(rx_dns, 0, sizeof(rx_dns));
    memcpy(&rx_dns[0], &dns_payload[0], 2);
    // Flags: Response, NXDOMAIN (RCODE = 3)
    rx_dns[2] = 0x81;
    rx_dns[3] = 0x83;
    rx_dns[4] = 0;
    rx_dns[5] = 1; // 1 Question
    rx_dns[6] = 0;
    rx_dns[7] = 0; // 0 Answers

    // Copy Question
    uint16_t udp_len_be;
    memcpy(&udp_len_be, &tx_frame[38], 2);
    uint16_t name_and_q_len = xNET_NTOHS(udp_len_be) - 8 - 12;
    memcpy(&rx_dns[12], &dns_payload[12], name_and_q_len);

    uint32_t rx_dns_len = 12 + name_and_q_len;

    // Inject reply
    uint8_t rx_frame[256];
    memcpy(&rx_frame[0], g_interface_ctx.mac_addr.addr, 6);
    memset(&rx_frame[6], 0x11, 6);
    rx_frame[12] = 0x08;
    rx_frame[13] = 0x00;

    rx_frame[14] = 0x45;
    rx_frame[15] = 0x00;
    uint16_t ip_total_len = 20 + 8 + rx_dns_len;
    rx_frame[16] = (uint8_t)((ip_total_len >> 8) & 0xFFU);
    rx_frame[17] = (uint8_t)(ip_total_len & 0xFFU);
    memset(&rx_frame[18], 0, 4);
    rx_frame[22] = 64;
    rx_frame[23] = 17;
    rx_frame[24] = 0;
    rx_frame[25] = 0;
    memcpy(&rx_frame[26], g_interface_ctx.dns_primary.addr, 4);
    memcpy(&rx_frame[30], g_interface_ctx.ip_addr.addr, 4);

    rx_frame[34] = 0x00;
    rx_frame[35] = 0x35;
    uint16_t dest_port_be = xNET_HTONS(dns_ctx.udp_socket.local_port);
    memcpy(&rx_frame[36], &dest_port_be, 2);
    uint16_t udp_total_len = 8 + rx_dns_len;
    rx_frame[38] = (uint8_t)((udp_total_len >> 8) & 0xFFU);
    rx_frame[39] = (uint8_t)(udp_total_len & 0xFFU);
    rx_frame[40] = 0;
    rx_frame[41] = 0;
    memcpy(&rx_frame[42], rx_dns, rx_dns_len);

    uint32_t rx_frame_len = 14 + ip_total_len;

    xRETURN_t rx_ret =
        xNET_Interface_RX_Frame(&g_interface_ctx, rx_frame, rx_frame_len, xNET_RX_FLAG_IP_CHECKSUM_VALID | xNET_RX_FLAG_L4_CHECKSUM_VALID);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, rx_ret);

    // Process RX
    (void)xNET_Process(&g_net_ctx);

    // Verify NXDOMAIN name error dispatched
    TEST_ASSERT_EQUAL_UINT32(1U, s_callback_count);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_DNS_NAME_ERROR, s_last_result);
    TEST_ASSERT_FALSE(dns_ctx.is_active);
    TEST_ASSERT_EQUAL(xNET_DNS_STATE_FAILED, dns_ctx.state);
}

void test_xnet_dns_timeout_and_retry(void)
{
    xNET_DNS_Context_t dns_ctx;
    (void)memset(&dns_ctx, 0, sizeof(dns_ctx));

    xRETURN_t ret = xNET_DNS_Query_A(&g_net_ctx, &dns_ctx, "google.com", mock_dns_callback);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, ret);

    (void)xNET_Process(&g_net_ctx);

    // Intercept original query
    uint8_t tx_frame[1000];
    uint32_t tx_len = 0;
    (void)xNET_Fake_Interface_TX_Pop(&g_fake_driver_ctx, tx_frame, sizeof(tx_frame), &tx_len);

    // Tick below timeout (1000 ms), query is 2000 ms timeout initially
    xNET_DNS_Tick(&dns_ctx, 1000U);
    (void)xNET_Process(&g_net_ctx);

    tx_len = 0;
    xRETURN_t tx_ret = xNET_Fake_Interface_TX_Pop(&g_fake_driver_ctx, tx_frame, sizeof(tx_frame), &tx_len);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NOT_FOUND, tx_ret); // No retransmission

    // Tick past timeout (+1500 ms, total 2500 ms) -> triggers first retry (total retry_count = 1)
    xNET_DNS_Tick(&dns_ctx, 1500U);
    (void)xNET_Process(&g_net_ctx);

    tx_len = 0;
    tx_ret = xNET_Fake_Interface_TX_Pop(&g_fake_driver_ctx, tx_frame, sizeof(tx_frame), &tx_len);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, tx_ret); // Retransmission sent!
    TEST_ASSERT_EQUAL_UINT8(1U, dns_ctx.retry_count);
    TEST_ASSERT_EQUAL_UINT32(4000U, dns_ctx.current_retry_timeout); // Backoff: 2000 * 2 = 4000U

    // Tick past second timeout (+4500 ms) -> triggers second retry (total retry_count = 2)
    xNET_DNS_Tick(&dns_ctx, 4500U);
    (void)xNET_Process(&g_net_ctx);

    tx_len = 0;
    (void)xNET_Fake_Interface_TX_Pop(&g_fake_driver_ctx, tx_frame, sizeof(tx_frame), &tx_len);
    TEST_ASSERT_EQUAL_UINT8(2U, dns_ctx.retry_count);
    TEST_ASSERT_EQUAL_UINT32(8000U, dns_ctx.current_retry_timeout);

    // Tick past third timeout (+8500 ms) -> triggers third retry (total retry_count = 3)
    xNET_DNS_Tick(&dns_ctx, 8500U);
    (void)xNET_Process(&g_net_ctx);

    tx_len = 0;
    (void)xNET_Fake_Interface_TX_Pop(&g_fake_driver_ctx, tx_frame, sizeof(tx_frame), &tx_len);
    TEST_ASSERT_EQUAL_UINT8(3U, dns_ctx.retry_count);
    TEST_ASSERT_EQUAL_UINT32(16000U, dns_ctx.current_retry_timeout);

    // Tick past fourth timeout (+16500 ms) -> retry limit is 3, so retries exhausted -> timeouts
    xNET_DNS_Tick(&dns_ctx, 16500U);
    (void)xNET_Process(&g_net_ctx);

    // Verify callback fires with timeout result
    TEST_ASSERT_EQUAL_UINT32(1U, s_callback_count);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_TIMEOUT, s_last_result);
    TEST_ASSERT_FALSE(dns_ctx.is_active);
    TEST_ASSERT_EQUAL(xNET_DNS_STATE_FAILED, dns_ctx.state);
}

// MAIN RUNNER /////////////////////////////////////////////////////////////////////

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    UNITY_BEGIN();
    RUN_TEST(test_xnet_dns_no_server);
    RUN_TEST(test_xnet_dns_busy);
    RUN_TEST(test_xnet_dns_query_success);
    RUN_TEST(test_xnet_dns_query_nxdomain);
    RUN_TEST(test_xnet_dns_timeout_and_retry);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
