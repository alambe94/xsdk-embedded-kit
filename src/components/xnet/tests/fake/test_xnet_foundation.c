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

// @file test_xnet_foundation.c
// @brief Host tests for the xNET foundation layer, including return codes, checksums, and init lifecycle.
//

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "unity.h"
#include "xnet_core.h"
#include "xnet_defs.h"
#include "xnet_config.h"
#include "xnet_return.h"
#include "xnet_interface_fake.h"
#include "xnet_ethernet.h"
#include "xnet_ipv4.h"
#include "xnet_icmp.h"
#include "xnet_udp.h"
#include "xnet_timer.h"
#include "xnet_dhcp.h"

// DEBUG

// MACROS //////////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////////

// EXTERN VARIABLES ////////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////
void setUp(void);
void tearDown(void);
void test_xnet_init_null_params(void);
void test_xnet_init_success(void);
void test_xnet_process_invalid_state(void);
void test_xnet_endian_swaps(void);
void test_xnet_checksum_simple(void);
void test_xnet_checksum_pseudo(void);
void test_xnet_interface_add_null_params(void);
void test_xnet_interface_add_success(void);
void test_xnet_interface_add_max_limit(void);
void test_xnet_interface_link_set(void);
void test_xnet_interface_process_poll(void);
void test_xnet_interface_rx_frame(void);
void test_xnet_packet_alloc_release_success(void);
void test_xnet_packet_alloc_limits(void);
void test_xnet_packet_helpers(void);
void test_xnet_packet_boundary_checks(void);
void test_xnet_fake_interface_lifecycle(void);
void test_xnet_fake_interface_tx_rx(void);
void test_xnet_fake_interface_limits(void);

// Overriding weak xassert_system_halt to prevent hanging on assertions in unit tests
void xassert_system_halt(void)
{
    // Do nothing: allows the test to continue and verify the return value
}

static uint32_t s_mock_transmit_count = 0;
static uint32_t s_mock_poll_count = 0;

static xRETURN_t mock_transmit(void *driver_ctx, const uint8_t *packet, uint32_t length, uint32_t tx_flags)
{
    (void)driver_ctx;
    (void)packet;
    (void)length;
    (void)tx_flags;
    s_mock_transmit_count++;
    return xRETURN_xNET_OK;
}

static xRETURN_t mock_poll(void *driver_ctx)
{
    (void)driver_ctx;
    s_mock_poll_count++;
    return xRETURN_xNET_OK;
}

static const xNET_Interface_Ops_t s_mock_ops = {.transmit = mock_transmit, .poll = mock_poll, .set_multicast_filter = NULL, .flush = NULL};

static void setup_arp_frame(uint8_t *buffer,
                            uint16_t ethertype,
                            uint16_t arp_op,
                            const xNET_MAC_Address_t *dest_mac,
                            const xNET_MAC_Address_t *src_mac,
                            const xNET_MAC_Address_t *sender_mac,
                            const xNET_IPv4_Address_t *sender_ip,
                            const xNET_MAC_Address_t *target_mac,
                            const xNET_IPv4_Address_t *target_ip)
{
    // Ethernet Header (14 bytes)
    memcpy(&buffer[0], dest_mac->addr, 6);
    memcpy(&buffer[6], src_mac->addr, 6);
    buffer[12] = (uint8_t)((ethertype >> 8) & 0xFFU);
    buffer[13] = (uint8_t)(ethertype & 0xFFU);

    if (ethertype == xNET_ETHERTYPE_ARP)
    {
        // ARP Body (28 bytes starting at offset 14)
        // HTYPE = 1 (Ethernet)
        buffer[14] = 0;
        buffer[15] = 1;
        // PTYPE = 0x0800 (IPv4)
        buffer[16] = 0x08;
        buffer[17] = 0x00;
        // HLEN = 6, PLEN = 4
        buffer[18] = 6;
        buffer[19] = 4;
        // OPER
        buffer[20] = (uint8_t)((arp_op >> 8) & 0xFFU);
        buffer[21] = (uint8_t)(arp_op & 0xFFU);
        // Sender MAC & IP
        memcpy(&buffer[22], sender_mac->addr, 6);
        memcpy(&buffer[28], sender_ip->addr, 4);
        // Target MAC & IP
        memcpy(&buffer[32], target_mac->addr, 6);
        memcpy(&buffer[38], target_ip->addr, 4);
    }
}

// MODULE FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

void setUp(void)
{
}

void tearDown(void)
{
}

void test_xnet_init_null_params(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};

    // NULL context
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Init(NULL, &net_config));

    // NULL config
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Init(&net_ctx, NULL));
}

void test_xnet_init_success(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];

    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));
    TEST_ASSERT_TRUE(net_ctx.is_initialized);
    TEST_ASSERT_EQUAL_PTR(pool, net_ctx.config.packet_pool_buffer);
    TEST_ASSERT_EQUAL_UINT32(sizeof(pool), net_ctx.config.packet_pool_buffer_size);

    // Verify alignment and block parsing
    for (uint32_t i = 0U; i < xNET_CONFIG_PACKET_POOL_SIZE; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(0U, (uintptr_t)net_ctx.packet_pool[i].buffer % 32U);
        TEST_ASSERT_EQUAL_UINT32(1536U, net_ctx.packet_pool[i].capacity);
        TEST_ASSERT_FALSE(net_ctx.packet_pool[i].is_in_use);
    }
}

void test_xnet_process_invalid_state(void)
{
    xNET_Context_t net_ctx = {0};

    // Uninitialized context
    net_ctx.is_initialized = false;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_STATE, xNET_Process(&net_ctx));

    // NULL context
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Process(NULL));
}

void test_xnet_endian_swaps(void)
{
    // 16-bit swaps
    TEST_ASSERT_EQUAL_UINT16(0x3412U, xNET_HTONS(0x1234U));
    TEST_ASSERT_EQUAL_UINT16(0x1234U, xNET_NTOHS(0x3412U));

    // 32-bit swaps
    TEST_ASSERT_EQUAL_UINT32(0x78563412UL, xNET_HTONL(0x12345678UL));
    TEST_ASSERT_EQUAL_UINT32(0x12345678UL, xNET_NTOHL(0x78563412UL));
}

void test_xnet_checksum_simple(void)
{
    // Test with a known string
    const char *test_data = "Hello World!";
    uint16_t checksum = xNET_Checksum_Calculate(test_data, strlen(test_data));

    // Calculated using internet standard 1s complement checksum rules
    // H e l l o   W o r l d ! \0
    // 0x4865 0x6c6c 0x6f20 0x576f 0x726c 0x6421
    // sum = 0x4865 + 0x6c6c + 0x6f20 + 0x576f + 0x726c + 0x6421 = 0x251ED
    // carry = 0x251ED >> 16 = 2
    // sum = 0x51ED + 2 = 0x51EF
    // ~sum = 0xAE10
    TEST_ASSERT_EQUAL_UINT16(0xAE10U, checksum);

    // Checksum of NULL should be 0
    TEST_ASSERT_EQUAL_UINT16(0U, xNET_Checksum_Calculate(NULL, 10U));
}

void test_xnet_checksum_pseudo(void)
{
    const char *test_data = "Ping";
    xNET_IPv4_Address_t src = {{192U, 168U, 1U, 100U}};
    xNET_IPv4_Address_t dest = {{192U, 168U, 1U, 1U}};
    uint8_t protocol = 17U;   // UDP
    uint16_t proto_len = 12U; // 8 bytes UDP header + 4 bytes payload

    uint16_t checksum = xNET_Checksum_Calculate_Pseudo(test_data, strlen(test_data), &src, &dest, protocol, proto_len);

    // Values:
    // src: 192.168.1.100 -> 0xC0A8, 0x0164
    // dest: 192.168.1.1 -> 0xC0A8, 0x0101
    // protocol: 17 (0x11), proto_len: 12 (0x0C) -> 0x0011 + 0x000C = 0x001D
    // payload: 'P' 'i' 'n' 'g' -> 0x5069, 0x6E67
    // sum = 0xC0A8 + 0x0164 + 0xC0A8 + 0x0101 + 0x0011 + 0x000C + 0x5069 + 0x6E67 = 0x242A2
    // carry = 2
    // sum = 0x42A2 + 2 = 0x42A4
    // ~sum = 0xBD5B
    TEST_ASSERT_EQUAL_UINT16(0xBD5BU, checksum);
}

void test_xnet_interface_add_null_params(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Interface_Ops_t bad_ops = s_mock_ops;

    net_ctx.is_initialized = true;

    // NULL context
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Interface_Add(NULL, &interface_ctx));

    // NULL interface
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Interface_Add(&net_ctx, NULL));

    // NULL ops
    interface_ctx.ops = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Interface_Add(&net_ctx, &interface_ctx));

    // NULL transmit op
    bad_ops.transmit = NULL;
    interface_ctx.ops = &bad_ops;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Interface_Add(&net_ctx, &interface_ctx));

    // NULL poll op
    bad_ops = s_mock_ops;
    bad_ops.poll = NULL;
    interface_ctx.ops = &bad_ops;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Interface_Add(&net_ctx, &interface_ctx));
}

void test_xnet_interface_add_success(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Interface_Context_t interface_ctx = {0};

    net_ctx.is_initialized = true;
    interface_ctx.ops = &s_mock_ops;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, net_ctx.interface_count);
    TEST_ASSERT_EQUAL_PTR(&interface_ctx, net_ctx.interface_list);
    TEST_ASSERT_EQUAL_UINT32(xNET_INTERFACE_STATE_DOWN, interface_ctx.state);
}

void test_xnet_interface_add_max_limit(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Interface_Context_t if_ctx1 = {0};
    xNET_Interface_Context_t if_ctx2 = {0};
    xNET_Interface_Context_t if_ctx3 = {0};

    net_ctx.is_initialized = true;
    if_ctx1.ops = &s_mock_ops;
    if_ctx2.ops = &s_mock_ops;
    if_ctx3.ops = &s_mock_ops;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &if_ctx1));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &if_ctx2));

    // Third add exceeds xNET_CONFIG_MAX_INTERFACES (default 2)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_BUFFER_TOO_SMALL, xNET_Interface_Add(&net_ctx, &if_ctx3));
    TEST_ASSERT_EQUAL_UINT32(2U, net_ctx.interface_count);
}

void test_xnet_interface_link_set(void)
{
    xNET_Interface_Context_t interface_ctx = {0};

    // NULL interface check
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Interface_Link_Set(NULL, true));

    // Success up
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));
    TEST_ASSERT_EQUAL_UINT32(xNET_INTERFACE_STATE_UP, interface_ctx.state);

    // Success down
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, false));
    TEST_ASSERT_EQUAL_UINT32(xNET_INTERFACE_STATE_DOWN, interface_ctx.state);
}

void test_xnet_interface_process_poll(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Interface_Context_t interface_ctx = {0};

    net_ctx.is_initialized = true;
    interface_ctx.ops = &s_mock_ops;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));

    s_mock_poll_count = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Process(&net_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, s_mock_poll_count);
}

void test_xnet_interface_rx_frame(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];

    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    interface_ctx.ops = &s_mock_ops;
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));

    uint8_t buffer[64] = {0};

    // NULL interface check
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Interface_RX_Frame(NULL, buffer, sizeof(buffer), 0U));

    // NULL frame check
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Interface_RX_Frame(&interface_ctx, NULL, sizeof(buffer), 0U));

    // Short frame length check
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_LENGTH, xNET_Interface_RX_Frame(&interface_ctx, buffer, 10U, 0U));

    // Setup valid broadcast ARP frame in buffer
    xNET_MAC_Address_t dest = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    xNET_MAC_Address_t src = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    xNET_IPv4_Address_t sender_ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t zero_mac = {{0, 0, 0, 0, 0, 0}};

    setup_arp_frame(buffer, xNET_ETHERTYPE_ARP, xNET_ARP_OP_REQUEST, &dest, &src, &src, &sender_ip, &zero_mac, &interface_ctx.ip_addr);

    // Success check
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_RX_Frame(&interface_ctx, buffer, sizeof(buffer), 0U));
}

void test_xnet_packet_alloc_release_success(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];

    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Packet_Buffer_t *pkt1 = NULL;
    xNET_Packet_Buffer_t *pkt2 = NULL;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt1));
    TEST_ASSERT_NOT_NULL(pkt1);
    TEST_ASSERT_TRUE(pkt1->is_in_use);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt2));
    TEST_ASSERT_NOT_NULL(pkt2);
    TEST_ASSERT_TRUE(pkt2->is_in_use);
    TEST_ASSERT_NOT_EQUAL(pkt1, pkt2);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Release(&net_ctx, pkt1));
    TEST_ASSERT_FALSE(pkt1->is_in_use);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Release(&net_ctx, pkt2));
    TEST_ASSERT_FALSE(pkt2->is_in_use);
}

void test_xnet_packet_alloc_limits(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    xNET_Packet_Buffer_t *packets[xNET_CONFIG_PACKET_POOL_SIZE];

    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    for (uint32_t i = 0U; i < xNET_CONFIG_PACKET_POOL_SIZE; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &packets[i]));
        TEST_ASSERT_NOT_NULL(packets[i]);
    }

    xNET_Packet_Buffer_t *fail_pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NO_PACKET_BUFFER, xNET_Packet_Alloc(&net_ctx, &fail_pkt));
    TEST_ASSERT_NULL(fail_pkt);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Release(&net_ctx, packets[0]));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &fail_pkt));
    TEST_ASSERT_EQUAL_PTR(packets[0], fail_pkt);
}

void test_xnet_packet_helpers(void)
{
    xNET_Packet_Buffer_t packet = {0};
    static uint8_t raw_buffer[100];

    packet.buffer = raw_buffer;
    packet.capacity = sizeof(raw_buffer);
    packet.data_offset = 20U;
    packet.data_length = 50U;

    TEST_ASSERT_EQUAL_PTR(raw_buffer + 20U, xNET_Packet_Get_Data(&packet));
    TEST_ASSERT_EQUAL_UINT32(50U, xNET_Packet_Get_Length(&packet));
    TEST_ASSERT_EQUAL_UINT32(sizeof(raw_buffer), xNET_Packet_Get_Capacity(&packet));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_LENGTH, xNET_Packet_Set_Length(&packet, 100U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Set_Length(&packet, 30U));
    TEST_ASSERT_EQUAL_UINT32(30U, packet.data_length);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_BUFFER_TOO_SMALL, xNET_Packet_Push(&packet, 30U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Push(&packet, 10U));
    TEST_ASSERT_EQUAL_UINT32(10U, packet.data_offset);
    TEST_ASSERT_EQUAL_UINT32(40U, packet.data_length);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_LENGTH, xNET_Packet_Pull(&packet, 50U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Pull(&packet, 15U));
    TEST_ASSERT_EQUAL_UINT32(25U, packet.data_offset);
    TEST_ASSERT_EQUAL_UINT32(25U, packet.data_length);
}

void test_xnet_packet_boundary_checks(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Packet_Buffer_t packet = {0};
    xNET_Packet_Buffer_t *pkt_ptr = NULL;

    net_ctx.is_initialized = true;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Packet_Alloc(NULL, &pkt_ptr));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Packet_Alloc(&net_ctx, NULL));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Packet_Release(NULL, &packet));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Packet_Release(&net_ctx, NULL));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_ARGUMENT, xNET_Packet_Release(&net_ctx, &packet));

    TEST_ASSERT_NULL(xNET_Packet_Get_Data(NULL));
    TEST_ASSERT_EQUAL_UINT32(0U, xNET_Packet_Get_Length(NULL));
    TEST_ASSERT_EQUAL_UINT32(0U, xNET_Packet_Get_Capacity(NULL));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Packet_Set_Length(NULL, 10U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Packet_Push(NULL, 10U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Packet_Pull(NULL, 10U));
}

void test_xnet_fake_interface_lifecycle(void)
{
    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};

    // NULL checks
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Fake_Interface_Init(NULL, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Fake_Interface_Init(&fake_ctx, NULL));

    // Success init
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_PTR(&fake_ctx, interface_ctx.driver_ctx);
    TEST_ASSERT_EQUAL_PTR(xNET_Fake_Interface_Get_Ops(), interface_ctx.ops);
}

void test_xnet_fake_interface_tx_rx(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    static uint8_t pool[16384];

    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));

    // 1. Outgoing TX capture test
    uint8_t tx_frame[] = "Frame Transmit Data Test";
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, interface_ctx.ops->transmit(&fake_ctx, tx_frame, sizeof(tx_frame), 0U));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.tx_count);

    // Pop the captured TX frame
    uint8_t popped_frame[64] = {0};
    uint32_t popped_len = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, popped_frame, sizeof(popped_frame), &popped_len));
    TEST_ASSERT_EQUAL_UINT32(sizeof(tx_frame), popped_len);
    TEST_ASSERT_EQUAL_MEMORY(tx_frame, popped_frame, popped_len);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_ctx.tx_count);

    // TX fail injection
    fake_ctx.inject_tx_fail = true;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_LINK_DOWN, interface_ctx.ops->transmit(&fake_ctx, tx_frame, sizeof(tx_frame), 0U));
    fake_ctx.inject_tx_fail = false;

    // 2. Incoming RX injection test
    uint8_t rx_frame[64] = {0};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};

    xNET_MAC_Address_t dest = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    xNET_MAC_Address_t src = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    xNET_IPv4_Address_t sender_ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t zero_mac = {{0, 0, 0, 0, 0, 0}};

    setup_arp_frame(rx_frame, xNET_ETHERTYPE_ARP, xNET_ARP_OP_REQUEST, &dest, &src, &src, &sender_ip, &zero_mac, &interface_ctx.ip_addr);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_RX(&interface_ctx, rx_frame, sizeof(rx_frame)));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.rx_count);

    // Run process loop, which polls the driver and delivers packet via RX_Frame
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Process(&net_ctx));
    TEST_ASSERT_EQUAL_UINT32(0U, fake_ctx.rx_count); // Successfully flushed
}

void test_xnet_fake_interface_limits(void)
{
    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    uint8_t dummy_frame[64] = {0};
    memset(dummy_frame, 0xBB, sizeof(dummy_frame));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));

    // RX limit tests
    for (uint32_t i = 0U; i < xNET_FAKE_RX_QUEUE_DEPTH; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_RX(&interface_ctx, dummy_frame, sizeof(dummy_frame)));
    }
    // Next one overflows
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_BUFFER_TOO_SMALL, xNET_Fake_Interface_RX(&interface_ctx, dummy_frame, sizeof(dummy_frame)));

    // TX limit tests
    for (uint32_t i = 0U; i < xNET_FAKE_TX_QUEUE_DEPTH; i++)
    {
        TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, interface_ctx.ops->transmit(&fake_ctx, dummy_frame, sizeof(dummy_frame), 0U));
    }
    // Next one overflows
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_BUFFER_TOO_SMALL,
                             interface_ctx.ops->transmit(&fake_ctx, dummy_frame, sizeof(dummy_frame), 0U));
}

void test_xnet_ethernet_helpers(void)
{
    xNET_MAC_Address_t broadcast = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    xNET_MAC_Address_t unicast = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    xNET_MAC_Address_t multicast = {{0x01, 0x00, 0x5E, 0x00, 0x00, 0x01}};
    xNET_MAC_Address_t multicast2 = {{0x33, 0x33, 0x00, 0x00, 0x00, 0x01}};

    // Broadcast checks
    TEST_ASSERT_TRUE(xNET_Ethernet_Is_Broadcast(&broadcast));
    TEST_ASSERT_FALSE(xNET_Ethernet_Is_Broadcast(&unicast));
    TEST_ASSERT_FALSE(xNET_Ethernet_Is_Broadcast(&multicast));

    // Multicast checks
    TEST_ASSERT_TRUE(xNET_Ethernet_Is_Multicast(&multicast));
    TEST_ASSERT_TRUE(xNET_Ethernet_Is_Multicast(&multicast2));
    TEST_ASSERT_TRUE(xNET_Ethernet_Is_Multicast(&broadcast)); // broadcast has multicast bit set
    TEST_ASSERT_FALSE(xNET_Ethernet_Is_Multicast(&unicast));
}

void test_xnet_ethernet_parse_build(void)
{
    uint8_t buffer[64] = {0};
    xNET_MAC_Address_t dest = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    xNET_MAC_Address_t src = {{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    uint16_t ethertype = 0x0800U;

    // Build header
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Ethernet_Build(buffer, sizeof(buffer), &dest, &src, ethertype));

    // Check raw bytes
    TEST_ASSERT_EQUAL_MEMORY(dest.addr, &buffer[0], 6);
    TEST_ASSERT_EQUAL_MEMORY(src.addr, &buffer[6], 6);
    TEST_ASSERT_EQUAL_UINT8(0x08, buffer[12]);
    TEST_ASSERT_EQUAL_UINT8(0x00, buffer[13]);

    // Parse header
    xNET_MAC_Address_t parsed_dest = {0};
    xNET_MAC_Address_t parsed_src = {0};
    uint16_t parsed_type = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Ethernet_Parse(buffer, sizeof(buffer), &parsed_dest, &parsed_src, &parsed_type));

    TEST_ASSERT_EQUAL_MEMORY(dest.addr, parsed_dest.addr, 6);
    TEST_ASSERT_EQUAL_MEMORY(src.addr, parsed_src.addr, 6);
    TEST_ASSERT_EQUAL_UINT16(ethertype, parsed_type);

    // Error cases
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER,
                             xNET_Ethernet_Parse(NULL, sizeof(buffer), &parsed_dest, &parsed_src, &parsed_type));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_LENGTH, xNET_Ethernet_Parse(buffer, 10U, &parsed_dest, &parsed_src, &parsed_type));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_BUFFER_TOO_SMALL, xNET_Ethernet_Build(buffer, 10U, &dest, &src, ethertype));
}

void test_xnet_ethernet_rx_filtering(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    interface_ctx.ops = &s_mock_ops;
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));

    xNET_MAC_Address_t local_mac = interface_ctx.mac_addr;
    xNET_MAC_Address_t mismatch_mac = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    xNET_MAC_Address_t broadcast_mac = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    xNET_MAC_Address_t multicast_mac = {{0x01, 0x00, 0x5E, 0x00, 0x00, 0x01}};

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t zero_mac = {{0, 0, 0, 0, 0, 0}};

    // 1. Unicast mismatch frame
    uint8_t mismatch_frame[64] = {0};
    setup_arp_frame(mismatch_frame, xNET_ETHERTYPE_ARP, xNET_ARP_OP_REQUEST, &mismatch_mac, &local_mac, &local_mac, &interface_ctx.ip_addr,
                    &zero_mac, &remote_ip);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NOT_FOUND,
                             xNET_Interface_RX_Frame(&interface_ctx, mismatch_frame, sizeof(mismatch_frame), 0U));

    // 2. Unicast match frame
    uint8_t match_frame[64] = {0};
    setup_arp_frame(match_frame, xNET_ETHERTYPE_ARP, xNET_ARP_OP_REQUEST, &local_mac, &mismatch_mac, &mismatch_mac, &remote_ip, &zero_mac,
                    &interface_ctx.ip_addr);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_RX_Frame(&interface_ctx, match_frame, sizeof(match_frame), 0U));

    // 3. Broadcast match frame
    uint8_t broadcast_frame[64] = {0};
    setup_arp_frame(broadcast_frame, xNET_ETHERTYPE_ARP, xNET_ARP_OP_REQUEST, &broadcast_mac, &mismatch_mac, &mismatch_mac, &remote_ip,
                    &zero_mac, &interface_ctx.ip_addr);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_RX_Frame(&interface_ctx, broadcast_frame, sizeof(broadcast_frame), 0U));

    // 4. Multicast match frame
    uint8_t multicast_frame[64] = {0};
    setup_arp_frame(multicast_frame, xNET_ETHERTYPE_ARP, xNET_ARP_OP_REQUEST, &multicast_mac, &mismatch_mac, &mismatch_mac, &remote_ip,
                    &zero_mac, &interface_ctx.ip_addr);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_RX_Frame(&interface_ctx, multicast_frame, sizeof(multicast_frame), 0U));
}

void test_xnet_ethernet_tx_padding(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    static uint8_t pool[16384];

    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_Packet_Buffer_t *packet = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &packet));

    packet->data_offset = 32U;
    packet->data_length = 10U;
    uint8_t *payload = xNET_Packet_Get_Data(packet);
    memset(payload, 0x55, 10);

    xNET_MAC_Address_t dest = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    uint16_t ethertype = 0x0800U;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Ethernet_TX(&interface_ctx, packet, &dest, ethertype));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.tx_count);

    uint8_t captured[128] = {0};
    uint32_t captured_len = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &captured_len));

    TEST_ASSERT_EQUAL_UINT32(60U, captured_len);

    TEST_ASSERT_EQUAL_MEMORY(dest.addr, &captured[0], 6);
    TEST_ASSERT_EQUAL_MEMORY(interface_ctx.mac_addr.addr, &captured[6], 6);
    TEST_ASSERT_EQUAL_UINT8(0x08, captured[12]);
    TEST_ASSERT_EQUAL_UINT8(0x00, captured[13]);

    for (uint32_t i = 14U; i < 24U; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0x55, captured[i]);
    }
    for (uint32_t i = 24U; i < 60U; i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0x00, captured[i]);
    }
}

static xRETURN_t local_dummy_udp_cb(xNET_UDP_Context_t *udp_ctx,
                                    const xNET_IPv4_Address_t *remote_addr,
                                    uint16_t remote_port,
                                    const uint8_t *data,
                                    uint32_t length)
{
    (void)udp_ctx;
    (void)remote_addr;
    (void)remote_port;
    (void)data;
    (void)length;
    return xRETURN_xNET_OK;
}

void test_xnet_ethernet_routing(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    interface_ctx.ops = &s_mock_ops;
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));

    // Open UDP socket so UDP routing succeeds
    xNET_UDP_Context_t udp_socket = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Open(&net_ctx, &udp_socket, 12345U, local_dummy_udp_cb));

    xNET_MAC_Address_t broadcast_mac = {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    xNET_MAC_Address_t remote_mac = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t zero_mac = {{0, 0, 0, 0, 0, 0}};

    // 1. ARP Frame
    uint8_t arp_frame[64] = {0};
    setup_arp_frame(arp_frame, xNET_ETHERTYPE_ARP, xNET_ARP_OP_REQUEST, &broadcast_mac, &remote_mac, &remote_mac, &remote_ip, &zero_mac,
                    &interface_ctx.ip_addr);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_RX_Frame(&interface_ctx, arp_frame, sizeof(arp_frame), 0U));

    // 2. IPv4 Frame (routed to IPv4 -> returns OK because UDP/ICMP are now implemented and route to open sockets)
    uint8_t ipv4_frame[64] = {0};
    memcpy(&ipv4_frame[0], broadcast_mac.addr, 6);
    memcpy(&ipv4_frame[6], remote_mac.addr, 6);
    ipv4_frame[12] = (uint8_t)((xNET_ETHERTYPE_IPV4 >> 8) & 0xFFU);
    ipv4_frame[13] = (uint8_t)(xNET_ETHERTYPE_IPV4 & 0xFFU);
    ipv4_frame[14] = 0x45U; // Version = 4, IHL = 5
    ipv4_frame[15] = 0x00U; // TOS
    ipv4_frame[16] = 0x00U;
    ipv4_frame[17] = 28U; // Total length = 28 (20 IP + 8 UDP)
    ipv4_frame[18] = 0x00U;
    ipv4_frame[19] = 0x00U; // ID
    ipv4_frame[20] = 0x00U;
    ipv4_frame[21] = 0x00U;                  // Flags/Offset
    ipv4_frame[22] = 64U;                    // TTL
    ipv4_frame[23] = xNET_IPV4_PROTOCOL_UDP; // Protocol
    ipv4_frame[24] = 0x00U;
    ipv4_frame[25] = 0x00U;                                 // Checksum
    memcpy(&ipv4_frame[26], remote_ip.addr, 4);             // Src IP
    memcpy(&ipv4_frame[30], interface_ctx.ip_addr.addr, 4); // Dest IP
    // UDP Header starting at index 34
    ipv4_frame[34] = 0x30U; // Src Port = 12345 (high)
    ipv4_frame[35] = 0x39U; // Src Port = 12345 (low)
    ipv4_frame[36] = 0x30U; // Dest Port = 12345 (high)
    ipv4_frame[37] = 0x39U; // Dest Port = 12345 (low)
    ipv4_frame[38] = 0x00U; // UDP length = 8 (high)
    ipv4_frame[39] = 0x08U; // UDP length = 8 (low)
    ipv4_frame[40] = 0x00U; // UDP Checksum = 0 (high)
    ipv4_frame[41] = 0x00U; // UDP Checksum = 0 (low)

    uint16_t checksum = xNET_Checksum_Calculate(&ipv4_frame[14], 20U);
    ipv4_frame[24] = (uint8_t)((checksum >> 8) & 0xFFU);
    ipv4_frame[25] = (uint8_t)(checksum & 0xFFU);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_RX_Frame(&interface_ctx, ipv4_frame, sizeof(ipv4_frame), 0U));

    // 3. Unsupported EtherType Frame
    uint8_t unknown_frame[64] = {0};
    setup_arp_frame(unknown_frame, 0x88F7, 0U, &broadcast_mac, &remote_mac, &remote_mac, &remote_ip, &zero_mac, &interface_ctx.ip_addr);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_UNSUPPORTED,
                             xNET_Interface_RX_Frame(&interface_ctx, unknown_frame, sizeof(unknown_frame), 0U));
}

void test_xnet_arp_cache_query(void)
{
    xNET_Interface_Context_t interface_ctx = {0};
    xNET_ARP_Cache_Init(&interface_ctx);

    xNET_IPv4_Address_t ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t mac = {0};

    // Query on empty cache
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NOT_FOUND, xNET_ARP_Cache_Query(&interface_ctx, &ip, &mac));

    // Insert manually resolved entry
    interface_ctx.arp_cache[0].ip_addr = ip;
    interface_ctx.arp_cache[0].mac_addr = (xNET_MAC_Address_t){{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;

    // Query should succeed
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_ARP_Cache_Query(&interface_ctx, &ip, &mac));
    TEST_ASSERT_EQUAL_MEMORY(interface_ctx.arp_cache[0].mac_addr.addr, mac.addr, 6);
}

void test_xnet_arp_resolve(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};

    // 1. Resolve to unknown IP -> should create PENDING entry and transmit an ARP Request
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 50;
    memset(xNET_Packet_Get_Data(pkt), 0x99, 50);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_ARP_Resolve(&interface_ctx, &remote_ip, pkt));

    // Entry should be PENDING
    TEST_ASSERT_EQUAL_UINT32(xNET_ARP_ENTRY_PENDING, interface_ctx.arp_cache[0].state);
    TEST_ASSERT_EQUAL_PTR(pkt, interface_ctx.arp_cache[0].pending_packet);

    // ARP Request should be sent (captured in fake interface)
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.tx_count);
    uint8_t captured[128] = {0};
    uint32_t captured_len = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &captured_len));

    // Verify it's an ARP request (offset 12 is 0x08, offset 13 is 0x06, offset 20-21 is 0x0001)
    TEST_ASSERT_EQUAL_UINT8(0x08, captured[12]);
    TEST_ASSERT_EQUAL_UINT8(0x06, captured[13]);
    TEST_ASSERT_EQUAL_UINT8(0, captured[20]);
    TEST_ASSERT_EQUAL_UINT8(1, captured[21]);

    // 2. Resolve to the same IP while PENDING -> should replace the pending packet (releasing the old one)
    xNET_Packet_Buffer_t *pkt2 = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt2));
    pkt2->data_offset = 32;
    pkt2->data_length = 60;
    memset(xNET_Packet_Get_Data(pkt2), 0x88, 60);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_ARP_Resolve(&interface_ctx, &remote_ip, pkt2));
    TEST_ASSERT_EQUAL_PTR(pkt2, interface_ctx.arp_cache[0].pending_packet);
    TEST_ASSERT_FALSE(pkt->is_in_use); // pkt should be released
}

void test_xnet_arp_rx(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // Buffer a pending packet
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 50;
    memset(xNET_Packet_Get_Data(pkt), 0x55, 50);

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_ARP_Resolve(&interface_ctx, &remote_ip, pkt));

    // Reset captured TX count
    fake_ctx.tx_count = 0;
    fake_ctx.tx_write_idx = 0;
    fake_ctx.tx_read_idx = 0;

    // Inject an ARP Reply from remote
    uint8_t reply_frame[64] = {0};
    xNET_MAC_Address_t remote_mac = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    setup_arp_frame(reply_frame, xNET_ETHERTYPE_ARP, xNET_ARP_OP_REPLY, &interface_ctx.mac_addr, &remote_mac, &remote_mac, &remote_ip,
                    &interface_ctx.mac_addr, &interface_ctx.ip_addr);

    // Call RX_Frame
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_RX_Frame(&interface_ctx, reply_frame, sizeof(reply_frame), 0U));

    // Cache should be RESOLVED
    TEST_ASSERT_EQUAL_UINT32(xNET_ARP_ENTRY_RESOLVED, interface_ctx.arp_cache[0].state);

    // Pending packet should have been sent (captured in fake interface)
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.tx_count);
    uint8_t captured[128] = {0};
    uint32_t captured_len = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &captured_len));

    // Header destination should be remote_mac, source should be local_mac, type should be IPv4, and body should match our pending packet
    TEST_ASSERT_EQUAL_MEMORY(remote_mac.addr, &captured[0], 6);
    TEST_ASSERT_EQUAL_MEMORY(interface_ctx.mac_addr.addr, &captured[6], 6);
    TEST_ASSERT_EQUAL_UINT8(0x08, captured[12]);
    TEST_ASSERT_EQUAL_UINT8(0x00, captured[13]);
    TEST_ASSERT_EQUAL_UINT8(0x55, captured[14]);
}

void test_xnet_arp_cache_eviction(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // Populate all cache entries with RESOLVED entries
    for (uint32_t i = 0U; i < xNET_CONFIG_ARP_CACHE_SIZE; i++)
    {
        interface_ctx.arp_cache[i].ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, (uint8_t)(100 + i)}};
        interface_ctx.arp_cache[i].mac_addr = (xNET_MAC_Address_t){{0, 0, 0, 0, 0, (uint8_t)(100 + i)}};
        interface_ctx.arp_cache[i].state = xNET_ARP_ENTRY_RESOLVED;
        interface_ctx.arp_cache[i].timeout_ms = (i + 1U) * 10000U;
    }

    // Allocate packet for resolve
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 50;

    xNET_IPv4_Address_t new_ip = {{192, 168, 1, 200}};

    // Resolve will trigger eviction of the oldest resolved entry (i=0) to place a PENDING entry for new_ip
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_ARP_Resolve(&interface_ctx, &new_ip, pkt));

    // The entry with the smallest timeout (i=0, IP 192.168.1.100) should have been evicted!
    xNET_MAC_Address_t query_mac = {0};
    xNET_IPv4_Address_t evicted_ip = {{192, 168, 1, 100}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NOT_FOUND, xNET_ARP_Cache_Query(&interface_ctx, &evicted_ip, &query_mac));

    // The new one should be present (in PENDING state)
    TEST_ASSERT_EQUAL_UINT32(xNET_ARP_ENTRY_PENDING, interface_ctx.arp_cache[0].state);
    TEST_ASSERT_EQUAL_MEMORY(new_ip.addr, interface_ctx.arp_cache[0].ip_addr.addr, 4);
}

void test_xnet_arp_timeouts_retries(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // 1. Test PENDING retry retransmissions
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 50;

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_ARP_Resolve(&interface_ctx, &remote_ip, pkt));

    // Initial Request sent
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.tx_count);
    fake_ctx.tx_count = 0; // Reset count

    // Let's tick by 500ms
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 500U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Process(&net_ctx));
    TEST_ASSERT_EQUAL_UINT32(0U, fake_ctx.tx_count);

    // Let's tick by 600ms -> retransmit Request
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 600U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Process(&net_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.tx_count);
    fake_ctx.tx_count = 0; // Reset

    // We have 2 retries left. Let's exhaust all retries.
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 1100U)); // Retransmit 2
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Process(&net_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.tx_count);
    fake_ctx.tx_count = 0;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 1100U)); // Retransmit 3
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Process(&net_ctx));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_ctx.tx_count);
    fake_ctx.tx_count = 0;

    // Tick once more -> retries exhausted, entry should be freed, packet should be released
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 1100U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Process(&net_ctx));
    TEST_ASSERT_EQUAL_UINT32(xNET_ARP_ENTRY_FREE, interface_ctx.arp_cache[0].state);
    TEST_ASSERT_FALSE(pkt->is_in_use); // Released

    // 2. Test RESOLVED expiration
    xNET_MAC_Address_t remote_mac = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    interface_ctx.arp_cache[0].ip_addr = remote_ip;
    interface_ctx.arp_cache[0].mac_addr = remote_mac;
    interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;
    interface_ctx.arp_cache[0].timeout_ms = net_ctx.system_ticks + xNET_CONFIG_ARP_ENTRY_TIMEOUT_MS;
    TEST_ASSERT_EQUAL_UINT32(xNET_ARP_ENTRY_RESOLVED, interface_ctx.arp_cache[0].state);

    // Tick by 299 seconds -> still resolved
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 299000U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Process(&net_ctx));
    TEST_ASSERT_EQUAL_UINT32(xNET_ARP_ENTRY_RESOLVED, interface_ctx.arp_cache[0].state);

    // Tick by another 2 seconds -> expires
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 2000U));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Process(&net_ctx));
    TEST_ASSERT_EQUAL_UINT32(xNET_ARP_ENTRY_FREE, interface_ctx.arp_cache[0].state);
}

void test_xnet_timer_wrap_safe_comparisons(void)
{
    // Test basic values
    TEST_ASSERT_TRUE(xNET_TIME_BEFORE(100U, 200U));
    TEST_ASSERT_FALSE(xNET_TIME_BEFORE(200U, 100U));
    TEST_ASSERT_TRUE(xNET_TIME_AFTER(200U, 100U));
    TEST_ASSERT_FALSE(xNET_TIME_AFTER(100U, 200U));

    // Test inline functions
    TEST_ASSERT_TRUE(xNET_Time_Before(100U, 200U));
    TEST_ASSERT_FALSE(xNET_Time_Before(200U, 100U));
    TEST_ASSERT_TRUE(xNET_Time_After(200U, 100U));
    TEST_ASSERT_FALSE(xNET_Time_After(100U, 200U));

    TEST_ASSERT_TRUE(xNET_Time_Before_Or_Equal(100U, 100U));
    TEST_ASSERT_TRUE(xNET_Time_Before_Or_Equal(100U, 200U));
    TEST_ASSERT_FALSE(xNET_Time_Before_Or_Equal(200U, 100U));

    TEST_ASSERT_TRUE(xNET_Time_After_Or_Equal(100U, 100U));
    TEST_ASSERT_TRUE(xNET_Time_After_Or_Equal(200U, 100U));
    TEST_ASSERT_FALSE(xNET_Time_After_Or_Equal(100U, 200U));

    // Test rollover/wrapping conditions (32-bit uint32_t rollover)
    // 0xFFFFFFF0 is before 0x00000010 (difference is 32, which is < 2^31)
    TEST_ASSERT_TRUE(xNET_TIME_BEFORE(0xFFFFFFF0U, 0x00000010U));
    TEST_ASSERT_TRUE(xNET_Time_Before(0xFFFFFFF0U, 0x00000010U));
    TEST_ASSERT_TRUE(xNET_Time_Before_Or_Equal(0xFFFFFFF0U, 0x00000010U));

    TEST_ASSERT_TRUE(xNET_TIME_AFTER(0x00000010U, 0xFFFFFFF0U));
    TEST_ASSERT_TRUE(xNET_Time_After(0x00000010U, 0xFFFFFFF0U));
    TEST_ASSERT_TRUE(xNET_Time_After_Or_Equal(0x00000010U, 0xFFFFFFF0U));

    // Maximum distance comparison
    // 0x00000000 and 0x7FFFFFFF: 0x00000000 is before 0x7FFFFFFF
    TEST_ASSERT_TRUE(xNET_TIME_BEFORE(0x00000000U, 0x7FFFFFFFU));
    // 0x00000000 and 0x80000000: sign-bit wrap makes this evaluate to true
    TEST_ASSERT_TRUE(xNET_TIME_BEFORE(0x00000000U, 0x80000000U));
}

void test_xnet_timer_passive_tick(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // Verify system_ticks starts at 0
    TEST_ASSERT_EQUAL_UINT32(0U, net_ctx.system_ticks);

    // Tick by 100ms passively
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 100U));
    TEST_ASSERT_EQUAL_UINT32(100U, net_ctx.system_ticks);

    // Call passive tick with invalid arguments
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_Tick(NULL, 100U));

    xNET_Context_t uninit_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_STATE, xNET_Tick(&uninit_ctx, 100U));
}

void test_xnet_ipv4_rejects_malformed_headers(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // Test short length (< 20 bytes)
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 15;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_LENGTH, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);

    // Test invalid version (e.g. Version = 5)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 20;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    memset(data, 0, 20);
    data[0] = 0x55U; // version = 5, IHL = 5
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_PACKET, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);

    // Test short IHL (e.g. IHL = 4)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 20;
    data = xNET_Packet_Get_Data(pkt);
    memset(data, 0, 20);
    data[0] = 0x44U; // version = 4, IHL = 4
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_PACKET, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);

    // Test invalid checksum
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 20;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 20; // total length
    data[8] = 64; // TTL
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], interface_ctx.ip_addr.addr, 4);
    memcpy(&data[16], interface_ctx.ip_addr.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    uint16_t checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    data[11] ^= 0xFF; // Corrupt
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_CHECKSUM_FAILED, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);

    // Test HW checksum invalid
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 20;
    pkt->flags = xNET_RX_FLAG_IP_CHECKSUM_INVALID;
    interface_ctx.checksum_caps = xNET_CHECKSUM_CAP_IP_RX;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 20;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], interface_ctx.ip_addr.addr, 4);
    memcpy(&data[16], interface_ctx.ip_addr.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_CHECKSUM_FAILED, xNET_IPv4_RX(&interface_ctx, pkt));
    interface_ctx.checksum_caps = 0U; // Reset
    (void)xNET_Packet_Release(&net_ctx, pkt);
}

void test_xnet_ipv4_rejects_fragments(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // MF flag set (flags_fragment = 0x2000)
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 20;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 20;
    data[6] = 0x20; // MF set
    data[7] = 0x00;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], interface_ctx.ip_addr.addr, 4);
    memcpy(&data[16], interface_ctx.ip_addr.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    uint16_t checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_UNSUPPORTED, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);

    // Non-zero fragment offset (flags_fragment = 0x0001)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 20;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 20;
    data[6] = 0x00;
    data[7] = 0x01; // offset = 1
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], interface_ctx.ip_addr.addr, 4);
    memcpy(&data[16], interface_ctx.ip_addr.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_UNSUPPORTED, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);
}

void test_xnet_ipv4_rejects_options(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // Setup header with IHL = 6 (24 bytes)
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 24;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x46U; // IHL = 6
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 24;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], interface_ctx.ip_addr.addr, 4);
    memcpy(&data[16], interface_ctx.ip_addr.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    uint16_t checksum = xNET_Checksum_Calculate(data, 24);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_UNSUPPORTED, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);
}

void test_xnet_ipv4_rx_filtering_and_routing(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // Open UDP socket so UDP routing succeeds
    xNET_UDP_Context_t udp_socket = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Open(&net_ctx, &udp_socket, 12345U, local_dummy_udp_cb));

    xNET_IPv4_Address_t local_ip = {{192, 168, 1, 10}};
    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    xNET_IPv4_Address_t bad_ip = {{192, 168, 1, 11}};
    xNET_IPv4_Address_t limited_broadcast = {{255, 255, 255, 255}};
    xNET_IPv4_Address_t multicast_ip = {{224, 0, 0, 1}};
    xNET_IPv4_Address_t subnet_broadcast = {{192, 168, 1, 255}};
    xNET_IPv4_Address_t bad_subnet_broadcast = {{192, 168, 2, 255}};

    // 1. Unicast match
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 28;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 28;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], remote_ip.addr, 4);
    memcpy(&data[16], local_ip.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    uint16_t checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    // UDP Header starting at offset 20
    data[20] = 0x30U;                                                             // Src Port = 12345 (high)
    data[21] = 0x39U;                                                             // Src Port = 12345 (low)
    data[22] = 0x30U;                                                             // Dest Port = 12345 (high)
    data[23] = 0x39U;                                                             // Dest Port = 12345 (low)
    data[24] = 0x00U;                                                             // UDP length = 8 (high)
    data[25] = 0x08U;                                                             // UDP length = 8 (low)
    data[26] = 0x00U;                                                             // UDP Checksum = 0 (high)
    data[27] = 0x00U;                                                             // UDP Checksum = 0 (low)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_IPv4_RX(&interface_ctx, pkt)); // routed and released

    // 2. Unicast mismatch
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 20;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 20;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], remote_ip.addr, 4);
    memcpy(&data[16], bad_ip.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NOT_FOUND, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);

    // 3. Limited broadcast
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 28;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 28;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], remote_ip.addr, 4);
    memcpy(&data[16], limited_broadcast.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    // UDP Header starting at offset 20
    data[20] = 0x30U; // Src Port = 12345 (high)
    data[21] = 0x39U; // Src Port = 12345 (low)
    data[22] = 0x30U; // Dest Port = 12345 (high)
    data[23] = 0x39U; // Dest Port = 12345 (low)
    data[24] = 0x00U; // UDP length = 8 (high)
    data[25] = 0x08U; // UDP length = 8 (low)
    data[26] = 0x00U; // UDP Checksum = 0 (high)
    data[27] = 0x00U; // UDP Checksum = 0 (low)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_IPv4_RX(&interface_ctx, pkt));

    // 4. Multicast
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 28;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 28;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], remote_ip.addr, 4);
    memcpy(&data[16], multicast_ip.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    // UDP Header starting at offset 20
    data[20] = 0x30U; // Src Port = 12345 (high)
    data[21] = 0x39U; // Src Port = 12345 (low)
    data[22] = 0x30U; // Dest Port = 12345 (high)
    data[23] = 0x39U; // Dest Port = 12345 (low)
    data[24] = 0x00U; // UDP length = 8 (high)
    data[25] = 0x08U; // UDP length = 8 (low)
    data[26] = 0x00U; // UDP Checksum = 0 (high)
    data[27] = 0x00U; // UDP Checksum = 0 (low)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_IPv4_RX(&interface_ctx, pkt));

    // 5. Subnet broadcast
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 28;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 28;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], remote_ip.addr, 4);
    memcpy(&data[16], subnet_broadcast.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    // UDP Header starting at offset 20
    data[20] = 0x30U; // Src Port = 12345 (high)
    data[21] = 0x39U; // Src Port = 12345 (low)
    data[22] = 0x30U; // Dest Port = 12345 (high)
    data[23] = 0x39U; // Dest Port = 12345 (low)
    data[24] = 0x00U; // UDP length = 8 (high)
    data[25] = 0x08U; // UDP length = 8 (low)
    data[26] = 0x00U; // UDP Checksum = 0 (high)
    data[27] = 0x00U; // UDP Checksum = 0 (low)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_IPv4_RX(&interface_ctx, pkt));

    // 6. Bad subnet broadcast
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 20;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 20;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], remote_ip.addr, 4);
    memcpy(&data[16], bad_subnet_broadcast.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NOT_FOUND, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);

    // 7. Unsupported protocol
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 20;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 20;
    data[8] = 64;
    data[9] = 99; // unsupported
    memcpy(&data[12], remote_ip.addr, 4);
    memcpy(&data[16], local_ip.addr, 4);
    data[10] = 0U;
    data[11] = 0U;
    checksum = xNET_Checksum_Calculate(data, 20);
    data[10] = (uint8_t)((checksum >> 8) & 0xFF);
    data[11] = (uint8_t)(checksum & 0xFF);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_UNSUPPORTED, xNET_IPv4_RX(&interface_ctx, pkt));
    (void)xNET_Packet_Release(&net_ctx, pkt);

    // 8. HW checksum valid bypasses software checksum
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 28;
    pkt->flags = xNET_RX_FLAG_IP_CHECKSUM_VALID;
    interface_ctx.checksum_caps = xNET_CHECKSUM_CAP_IP_RX;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x45U;
    data[1] = 0x00;
    data[2] = 0x00;
    data[3] = 28;
    data[8] = 64;
    data[9] = xNET_IPV4_PROTOCOL_UDP;
    memcpy(&data[12], remote_ip.addr, 4);
    memcpy(&data[16], local_ip.addr, 4);
    // corrupted header checksum field, but because HW checksum valid is set, it passes!
    data[10] = 0x00;
    data[11] = 0x00;
    // UDP Header starting at offset 20
    data[20] = 0x30U; // Src Port = 12345 (high)
    data[21] = 0x39U; // Src Port = 12345 (low)
    data[22] = 0x30U; // Dest Port = 12345 (high)
    data[23] = 0x39U; // Dest Port = 12345 (low)
    data[24] = 0x00U; // UDP length = 8 (high)
    data[25] = 0x08U; // UDP length = 8 (low)
    data[26] = 0x00U; // UDP Checksum = 0 (high)
    data[27] = 0x00U; // UDP Checksum = 0 (low)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_IPv4_RX(&interface_ctx, pkt));
    interface_ctx.checksum_caps = 0U; // Reset
}

void test_xnet_ipv4_tx_building(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    uint8_t captured[128];
    uint32_t cap_len = 0;
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xNET_Interface_Add(&net_ctx, &interface_ctx), xRETURN_xNET_OK);
    TEST_ASSERT_EQUAL_UINT32(xNET_Interface_Link_Set(&interface_ctx, true), xRETURN_xNET_OK);

    // Populate ARP cache so ARP resolve finishes immediately
    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t remote_mac = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    interface_ctx.arp_cache[0].ip_addr = remote_ip;
    interface_ctx.arp_cache[0].mac_addr = remote_mac;
    interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;
    interface_ctx.arp_cache[0].timeout_ms = 300000;

    // Allocate packet for TX payload
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32 + 20; // Room for eth (14) + ipv4 (20)
    pkt->data_length = 10;      // 10 bytes payload
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    memset(data, 0xA5, 10);

    // Send via IPv4 TX
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_IPv4_TX(&interface_ctx, pkt, &remote_ip, xNET_IPV4_PROTOCOL_UDP));

    // Verify Ethernet frame was captured
    TEST_ASSERT_EQUAL_UINT32(1, fake_ctx.tx_count);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));
    TEST_ASSERT_EQUAL_UINT32(60, cap_len); // 14 (eth) + 20 (ipv4) + 10 (payload) = 44 -> padded to 60!

    // Verify IPv4 header fields in captured frame (offset 14)
    TEST_ASSERT_EQUAL_UINT8(0x45, captured[14]); // version/IHL
    TEST_ASSERT_EQUAL_UINT8(0x00, captured[15]); // TOS
    uint16_t total_len = ((uint16_t)captured[16] << 8) | captured[17];
    TEST_ASSERT_EQUAL_UINT16(30, total_len);                       // 20 + 10 payload
    TEST_ASSERT_EQUAL_UINT8(64, captured[22]);                     // TTL
    TEST_ASSERT_EQUAL_UINT8(xNET_IPV4_PROTOCOL_UDP, captured[23]); // protocol
    TEST_ASSERT_EQUAL_UINT8(192, captured[26]);
    TEST_ASSERT_EQUAL_UINT8(168, captured[27]);
    TEST_ASSERT_EQUAL_UINT8(1, captured[28]);
    TEST_ASSERT_EQUAL_UINT8(10, captured[29]); // Src IP
    TEST_ASSERT_EQUAL_UINT8(192, captured[30]);
    TEST_ASSERT_EQUAL_UINT8(168, captured[31]);
    TEST_ASSERT_EQUAL_UINT8(1, captured[32]);
    TEST_ASSERT_EQUAL_UINT8(20, captured[33]); // Dest IP

    // Verify header checksum calculation
    uint16_t csum = xNET_Checksum_Calculate(&captured[14], 20);
    TEST_ASSERT_EQUAL_UINT16(0, csum);

    // Test HW checksum offload on TX
    interface_ctx.checksum_caps = xNET_CHECKSUM_CAP_IP_TX;
    fake_ctx.tx_count = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32 + 20;
    pkt->data_length = 10;
    data = xNET_Packet_Get_Data(pkt);
    memset(data, 0x5A, 10);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_IPv4_TX(&interface_ctx, pkt, &remote_ip, xNET_IPV4_PROTOCOL_UDP));
    TEST_ASSERT_EQUAL_UINT32(1, fake_ctx.tx_count);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    // Verify checksum field is 0 (delegated to hardware)
    TEST_ASSERT_EQUAL_UINT8(0, captured[24]);
    TEST_ASSERT_EQUAL_UINT8(0, captured[25]);
}

void test_xnet_icmp_rx_rejects_short_packet(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};

    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 7;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    memset(data, 0, 7);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_LENGTH, xNET_ICMP_RX(&interface_ctx, pkt, &remote_ip));
}

void test_xnet_icmp_rx_rejects_bad_checksum(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};

    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 8;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    memset(data, 0, 8);
    data[0] = xNET_ICMP_TYPE_ECHO_REQUEST;
    data[1] = 0;
    data[2] = 0xAB;
    data[3] = 0xCD;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_CHECKSUM_FAILED, xNET_ICMP_RX(&interface_ctx, pkt, &remote_ip));
}

void test_xnet_icmp_rx_drops_unsupported_types(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};

    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 8;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    memset(data, 0, 8);
    data[0] = 3;
    data[1] = 0;

    uint16_t checksum = xNET_Checksum_Calculate(data, 8);
    data[2] = (uint8_t)((checksum >> 8) & 0xFF);
    data[3] = (uint8_t)(checksum & 0xFF);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_UNSUPPORTED, xNET_ICMP_RX(&interface_ctx, pkt, &remote_ip));
}

void test_xnet_icmp_rx_echo_request_reply_in_place(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t remote_mac = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    interface_ctx.arp_cache[0].ip_addr = remote_ip;
    interface_ctx.arp_cache[0].mac_addr = remote_mac;
    interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;

    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32 + 20;
    pkt->data_length = 12;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    data[0] = xNET_ICMP_TYPE_ECHO_REQUEST;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0x12;
    data[5] = 0x34;
    data[6] = 0x56;
    data[7] = 0x78;
    data[8] = 0xAA;
    data[9] = 0xBB;
    data[10] = 0xCC;
    data[11] = 0xDD;

    uint16_t checksum = xNET_Checksum_Calculate(data, 12);
    data[2] = (uint8_t)((checksum >> 8) & 0xFF);
    data[3] = (uint8_t)(checksum & 0xFF);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_ICMP_RX(&interface_ctx, pkt, &remote_ip));

    TEST_ASSERT_EQUAL_UINT32(1, fake_ctx.tx_count);
    uint8_t captured[128];
    uint32_t cap_len = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));
    TEST_ASSERT_EQUAL_UINT32(60, cap_len);

    TEST_ASSERT_EQUAL_UINT8(xNET_IPV4_PROTOCOL_ICMP, captured[23]);
    TEST_ASSERT_EQUAL_UINT8(192, captured[26]);
    TEST_ASSERT_EQUAL_UINT8(168, captured[27]);
    TEST_ASSERT_EQUAL_UINT8(1, captured[28]);
    TEST_ASSERT_EQUAL_UINT8(10, captured[29]);
    TEST_ASSERT_EQUAL_UINT8(192, captured[30]);
    TEST_ASSERT_EQUAL_UINT8(168, captured[31]);
    TEST_ASSERT_EQUAL_UINT8(1, captured[32]);
    TEST_ASSERT_EQUAL_UINT8(20, captured[33]);

    TEST_ASSERT_EQUAL_UINT8(xNET_ICMP_TYPE_ECHO_REPLY, captured[34]);
    TEST_ASSERT_EQUAL_UINT8(0, captured[35]);
    TEST_ASSERT_EQUAL_UINT8(0x12, captured[38]);
    TEST_ASSERT_EQUAL_UINT8(0x34, captured[39]);
    TEST_ASSERT_EQUAL_UINT8(0x56, captured[40]);
    TEST_ASSERT_EQUAL_UINT8(0x78, captured[41]);
    TEST_ASSERT_EQUAL_UINT8(0xAA, captured[42]);
    TEST_ASSERT_EQUAL_UINT8(0xBB, captured[43]);
    TEST_ASSERT_EQUAL_UINT8(0xCC, captured[44]);
    TEST_ASSERT_EQUAL_UINT8(0xDD, captured[45]);

    uint16_t reply_csum = xNET_Checksum_Calculate(&captured[34], 12);
    TEST_ASSERT_EQUAL_UINT16(0, reply_csum);
}

void test_xnet_icmp_rx_echo_request_hardware_offload(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t remote_mac = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    interface_ctx.arp_cache[0].ip_addr = remote_ip;
    interface_ctx.arp_cache[0].mac_addr = remote_mac;
    interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;

    interface_ctx.checksum_caps = xNET_CHECKSUM_CAP_ICMP_RX | xNET_CHECKSUM_CAP_ICMP_TX;

    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32 + 20;
    pkt->data_length = 8;
    pkt->flags = xNET_RX_FLAG_L4_CHECKSUM_INVALID;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    data[0] = xNET_ICMP_TYPE_ECHO_REQUEST;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_CHECKSUM_FAILED, xNET_ICMP_RX(&interface_ctx, pkt, &remote_ip));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32 + 20;
    pkt->data_length = 8;
    pkt->flags = xNET_RX_FLAG_L4_CHECKSUM_VALID;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = xNET_ICMP_TYPE_ECHO_REQUEST;
    data[1] = 0;
    data[2] = 0xFF;
    data[3] = 0xFF;
    data[4] = 0x11;
    data[5] = 0x22;
    data[6] = 0x33;
    data[7] = 0x44;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_ICMP_RX(&interface_ctx, pkt, &remote_ip));

    TEST_ASSERT_EQUAL_UINT32(1, fake_ctx.tx_count);
    uint8_t captured[128];
    uint32_t cap_len = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    TEST_ASSERT_EQUAL_UINT8(0, captured[36]);
    TEST_ASSERT_EQUAL_UINT8(0, captured[37]);
}

static uint32_t s_udp_rx_count = 0U;
static xNET_IPv4_Address_t s_udp_rx_remote_ip = {{0}};
static uint16_t s_udp_rx_remote_port = 0U;
static uint8_t s_udp_rx_data[128] = {0};
static uint32_t s_udp_rx_data_len = 0U;

static xRETURN_t mock_udp_receive_cb(xNET_UDP_Context_t *udp_ctx,
                                     const xNET_IPv4_Address_t *remote_addr,
                                     uint16_t remote_port,
                                     const uint8_t *data,
                                     uint32_t length)
{
    (void)udp_ctx;
    s_udp_rx_count++;
    s_udp_rx_remote_ip = *remote_addr;
    s_udp_rx_remote_port = remote_port;
    if (length <= sizeof(s_udp_rx_data))
    {
        (void)memcpy(s_udp_rx_data, data, length);
        s_udp_rx_data_len = length;
    }
    return xRETURN_xNET_OK;
}

void test_xnet_udp_open_close_bind(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_UDP_Context_t socket1 = {0};
    xNET_UDP_Context_t socket2 = {0};
    xNET_UDP_Context_t socket3 = {0};

    // 1. Open socket1 on port 1234
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Open(&net_ctx, &socket1, 1234U, mock_udp_receive_cb));
    TEST_ASSERT_EQUAL_UINT16(1234U, socket1.local_port);

    // 2. Open socket2 on port 1234 should fail (duplicate)
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_ARGUMENT, xNET_UDP_Open(&net_ctx, &socket2, 1234U, mock_udp_receive_cb));

    // 3. Open socket2 on port 0 should ephemerally allocate
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Open(&net_ctx, &socket2, 0U, mock_udp_receive_cb));
    TEST_ASSERT_TRUE(socket2.local_port >= 49152U);

    // 4. Open socket3 on port 0 should ephemerally allocate a different port
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Open(&net_ctx, &socket3, 0U, mock_udp_receive_cb));
    TEST_ASSERT_TRUE(socket3.local_port >= 49152U);
    TEST_ASSERT_NOT_EQUAL_UINT16(socket2.local_port, socket3.local_port);

    // 5. Close socket1
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Close(&socket1));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NOT_FOUND, xNET_UDP_Close(&socket1));

    // Close socket2 and socket3
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Close(&socket2));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Close(&socket3));
}

void test_xnet_udp_rx_validation(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};

    // 1. Short UDP packet
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 7;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_LENGTH, xNET_UDP_RX(&interface_ctx, pkt, &remote_ip));

    // 2. Bad UDP length field
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 10;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0;
    data[5] = 15;
    data[6] = 0;
    data[7] = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_LENGTH, xNET_UDP_RX(&interface_ctx, pkt, &remote_ip));

    // 3. Bad checksum
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 8;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0;
    data[1] = 0;
    data[2] = 0;
    data[3] = 0;
    data[4] = 0;
    data[5] = 8;
    data[6] = 0xAA;
    data[7] = 0xBB;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_CHECKSUM_FAILED, xNET_UDP_RX(&interface_ctx, pkt, &remote_ip));

    // 4. Closed port drop
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 8;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x11;
    data[1] = 0x22;
    data[2] = 0x33;
    data[3] = 0x44;
    data[4] = 0;
    data[5] = 8;
    data[6] = 0;
    data[7] = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NOT_FOUND, xNET_UDP_RX(&interface_ctx, pkt, &remote_ip));
}

void test_xnet_udp_rx_delivery(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};

    xNET_UDP_Context_t socket = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Open(&net_ctx, &socket, 5000U, mock_udp_receive_cb));

    s_udp_rx_count = 0;
    s_udp_rx_remote_port = 0;
    s_udp_rx_data_len = 0;

    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32;
    pkt->data_length = 13;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x17;
    data[1] = 0x70; // src 6000
    data[2] = 0x13;
    data[3] = 0x88; // dest 5000
    data[4] = 0;
    data[5] = 13;
    data[6] = 0;
    data[7] = 0;
    (void)memcpy(&data[8], "HELLO", 5);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_RX(&interface_ctx, pkt, &remote_ip));

    TEST_ASSERT_EQUAL_UINT32(1, s_udp_rx_count);
    TEST_ASSERT_EQUAL_UINT16(6000U, s_udp_rx_remote_port);
    TEST_ASSERT_EQUAL_UINT32(5, s_udp_rx_data_len);
    TEST_ASSERT_EQUAL_MEMORY("HELLO", s_udp_rx_data, 5);
    TEST_ASSERT_EQUAL_MEMORY(remote_ip.addr, s_udp_rx_remote_ip.addr, 4);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Close(&socket));
}

void test_xnet_udp_tx_send_to(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t remote_mac = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    interface_ctx.arp_cache[0].ip_addr = remote_ip;
    interface_ctx.arp_cache[0].mac_addr = remote_mac;
    interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;

    xNET_UDP_Context_t socket = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Open(&net_ctx, &socket, 5000U, mock_udp_receive_cb));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Send_To(&socket, &remote_ip, 8000U, (const uint8_t *)"WORLD", 5));

    TEST_ASSERT_EQUAL_UINT32(1, fake_ctx.tx_count);
    uint8_t captured[128];
    uint32_t cap_len = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));
    TEST_ASSERT_EQUAL_UINT32(60, cap_len);

    TEST_ASSERT_EQUAL_MEMORY(remote_mac.addr, &captured[0], 6);
    TEST_ASSERT_EQUAL_MEMORY(interface_ctx.mac_addr.addr, &captured[6], 6);
    TEST_ASSERT_EQUAL_UINT8(0x08, captured[12]);
    TEST_ASSERT_EQUAL_UINT8(0x00, captured[13]);

    TEST_ASSERT_EQUAL_UINT8(xNET_IPV4_PROTOCOL_UDP, captured[23]);

    TEST_ASSERT_EQUAL_UINT8(0x13, captured[34]);
    TEST_ASSERT_EQUAL_UINT8(0x88, captured[35]);
    TEST_ASSERT_EQUAL_UINT8(0x1F, captured[36]);
    TEST_ASSERT_EQUAL_UINT8(0x40, captured[37]);
    TEST_ASSERT_EQUAL_UINT8(0, captured[38]);
    TEST_ASSERT_EQUAL_UINT8(13, captured[39]);

    TEST_ASSERT_EQUAL_MEMORY("WORLD", &captured[42], 5);

    uint16_t csum = xNET_Checksum_Calculate_Pseudo(&captured[34], 13, &interface_ctx.ip_addr, &remote_ip, xNET_IPV4_PROTOCOL_UDP, 13);
    TEST_ASSERT_EQUAL_UINT16(0, csum);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Close(&socket));
}

void test_xnet_udp_tx_rx_hardware_offload(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    interface_ctx.ip_addr = (xNET_IPv4_Address_t){{192, 168, 1, 10}};
    interface_ctx.netmask = (xNET_IPv4_Address_t){{255, 255, 255, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    xNET_IPv4_Address_t remote_ip = {{192, 168, 1, 20}};
    xNET_MAC_Address_t remote_mac = {{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    interface_ctx.arp_cache[0].ip_addr = remote_ip;
    interface_ctx.arp_cache[0].mac_addr = remote_mac;
    interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;

    interface_ctx.checksum_caps = xNET_CHECKSUM_CAP_UDP_RX | xNET_CHECKSUM_CAP_UDP_TX;

    xNET_UDP_Context_t socket = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Open(&net_ctx, &socket, 5000U, mock_udp_receive_cb));

    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32 + 20;
    pkt->data_length = 8;
    pkt->flags = xNET_RX_FLAG_L4_CHECKSUM_INVALID;
    uint8_t *data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x17;
    data[1] = 0x70;
    data[2] = 0x13;
    data[3] = 0x88;
    data[4] = 0;
    data[5] = 8;
    data[6] = 0xFF;
    data[7] = 0xFF;

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_CHECKSUM_FAILED, xNET_UDP_RX(&interface_ctx, pkt, &remote_ip));

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(&net_ctx, &pkt));
    pkt->data_offset = 32 + 20;
    pkt->data_length = 8;
    pkt->flags = xNET_RX_FLAG_L4_CHECKSUM_VALID;
    data = xNET_Packet_Get_Data(pkt);
    data[0] = 0x17;
    data[1] = 0x70;
    data[2] = 0x13;
    data[3] = 0x88;
    data[4] = 0;
    data[5] = 8;
    data[6] = 0xAB;
    data[7] = 0xCD;

    s_udp_rx_count = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_RX(&interface_ctx, pkt, &remote_ip));
    TEST_ASSERT_EQUAL_UINT32(1, s_udp_rx_count);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Send_To(&socket, &remote_ip, 8000U, (const uint8_t *)"HELLO", 5));
    TEST_ASSERT_EQUAL_UINT32(1, fake_ctx.tx_count);
    uint8_t captured[128];
    uint32_t cap_len = 0;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    TEST_ASSERT_EQUAL_UINT8(0, captured[40]);
    TEST_ASSERT_EQUAL_UINT8(0, captured[41]);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_Close(&socket));
}

void test_xnet_ipv4_config_static_success(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    interface_ctx.ops = &s_mock_ops;
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));

    xNET_IPv4_Address_t ip = {{192, 168, 1, 100}};
    xNET_IPv4_Address_t mask = {{255, 255, 255, 0}};
    xNET_IPv4_Address_t gw = {{192, 168, 1, 1}};

    // Verify successful config
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_IPv4_Config_Static(&interface_ctx, &ip, &mask, &gw));

    TEST_ASSERT_EQUAL_UINT8(192, interface_ctx.ip_addr.addr[0]);
    TEST_ASSERT_EQUAL_UINT8(168, interface_ctx.ip_addr.addr[1]);
    TEST_ASSERT_EQUAL_UINT8(1, interface_ctx.ip_addr.addr[2]);
    TEST_ASSERT_EQUAL_UINT8(100, interface_ctx.ip_addr.addr[3]);

    TEST_ASSERT_EQUAL_UINT8(255, interface_ctx.netmask.addr[0]);
    TEST_ASSERT_EQUAL_UINT8(255, interface_ctx.netmask.addr[1]);
    TEST_ASSERT_EQUAL_UINT8(255, interface_ctx.netmask.addr[2]);
    TEST_ASSERT_EQUAL_UINT8(0, interface_ctx.netmask.addr[3]);

    TEST_ASSERT_EQUAL_UINT8(192, interface_ctx.gateway.addr[0]);
    TEST_ASSERT_EQUAL_UINT8(168, interface_ctx.gateway.addr[1]);
    TEST_ASSERT_EQUAL_UINT8(1, interface_ctx.gateway.addr[2]);
    TEST_ASSERT_EQUAL_UINT8(1, interface_ctx.gateway.addr[3]);

    // DNS server fields must be zero by default
    TEST_ASSERT_EQUAL_UINT8(0, interface_ctx.dns_primary.addr[0]);
    TEST_ASSERT_EQUAL_UINT8(0, interface_ctx.dns_secondary.addr[0]);

    // Reconfigure with no gateway
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_IPv4_Config_Static(&interface_ctx, &ip, &mask, NULL));
    TEST_ASSERT_EQUAL_UINT8(0, interface_ctx.gateway.addr[0]);
    TEST_ASSERT_EQUAL_UINT8(0, interface_ctx.gateway.addr[1]);
    TEST_ASSERT_EQUAL_UINT8(0, interface_ctx.gateway.addr[2]);
    TEST_ASSERT_EQUAL_UINT8(0, interface_ctx.gateway.addr[3]);
}

void test_xnet_ipv4_config_static_validation(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    interface_ctx.ops = &s_mock_ops;
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));

    xNET_IPv4_Address_t valid_ip = {{192, 168, 1, 100}};
    xNET_IPv4_Address_t valid_mask = {{255, 255, 255, 0}};
    xNET_IPv4_Address_t valid_gw = {{192, 168, 1, 1}};

    // 1. Invalid local IP: Loopback (127.0.0.1)
    xNET_IPv4_Address_t loopback_ip = {{127, 0, 0, 1}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_ARGUMENT,
                             xNET_IPv4_Config_Static(&interface_ctx, &loopback_ip, &valid_mask, &valid_gw));

    // 2. Invalid local IP: Multicast (224.0.0.1)
    xNET_IPv4_Address_t multicast_ip = {{224, 0, 0, 1}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_ARGUMENT,
                             xNET_IPv4_Config_Static(&interface_ctx, &multicast_ip, &valid_mask, &valid_gw));

    // 3. Invalid local IP: Broadcast (255.255.255.255)
    xNET_IPv4_Address_t broadcast_ip = {{255, 255, 255, 255}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_ARGUMENT,
                             xNET_IPv4_Config_Static(&interface_ctx, &broadcast_ip, &valid_mask, &valid_gw));

    // 4. Invalid local IP: Unspecified (0.0.0.0)
    xNET_IPv4_Address_t zero_ip = {{0, 0, 0, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_ARGUMENT, xNET_IPv4_Config_Static(&interface_ctx, &zero_ip, &valid_mask, &valid_gw));

    // 5. Invalid subnet mask: non-contiguous bits (255.255.255.1)
    xNET_IPv4_Address_t bad_mask = {{255, 255, 255, 1}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_ARGUMENT, xNET_IPv4_Config_Static(&interface_ctx, &valid_ip, &bad_mask, &valid_gw));

    // 6. Invalid subnet mask: all-zero
    xNET_IPv4_Address_t zero_mask = {{0, 0, 0, 0}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_ARGUMENT, xNET_IPv4_Config_Static(&interface_ctx, &valid_ip, &zero_mask, &valid_gw));

    // 7. Gateway subnet mismatch: gateway is 192.168.2.1 for 192.168.1.100/24
    xNET_IPv4_Address_t bad_gw = {{192, 168, 2, 1}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_INVALID_ARGUMENT, xNET_IPv4_Config_Static(&interface_ctx, &valid_ip, &valid_mask, &bad_gw));
}

void test_xnet_ipv4_config_static_null(void)
{
    xNET_IPv4_Address_t ip = {{192, 168, 1, 100}};
    xNET_IPv4_Address_t mask = {{255, 255, 255, 0}};

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xNET_NULL_POINTER, xNET_IPv4_Config_Static(NULL, &ip, &mask, NULL));
}

static void
verify_sent_dhcp_msg(xNET_Fake_Interface_Context_t *fake_ctx, uint8_t expected_msg_type, const xNET_IPv4_Address_t *expected_dest_ip)
{
    uint8_t captured[512];
    (void)memset(captured, 0, sizeof(captured));
    uint32_t cap_len = 0U;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(fake_ctx, captured, sizeof(captured), &cap_len));

    if (cap_len < 14U + 20U + 8U + 240U)
    {
        printf("DEBUG verify_sent_dhcp_msg: cap_len = %u, ethertype = 0x%02X%02X\n", cap_len, captured[12], captured[13]);
        if (cap_len >= 34U)
        {
            printf("DEBUG dest_ip = %u.%u.%u.%u, proto = %u\n", captured[30], captured[31], captured[32], captured[33], captured[23]);
        }
    }

    // We expect an Ethernet II frame (14 bytes) + IPv4 Header (20 bytes) + UDP Header (8 bytes) + DHCP payload (at least 240 bytes)
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(14U + 20U + 8U + 240U, cap_len);

    // Check ethertype is IPv4 (0x0800)
    TEST_ASSERT_EQUAL_UINT8(0x08U, captured[12]);
    TEST_ASSERT_EQUAL_UINT8(0x00U, captured[13]);

    // Check IP Dest
    if (expected_dest_ip != NULL)
    {
        TEST_ASSERT_EQUAL_MEMORY(expected_dest_ip->addr, &captured[14U + 16U], 4);
    }

    // Check UDP Dest port (67 = 0x0043)
    TEST_ASSERT_EQUAL_UINT8(0x00U, captured[14U + 20U + 2U]);
    TEST_ASSERT_EQUAL_UINT8(0x43U, captured[14U + 20U + 3U]);

    // Check DHCP op (BOOTREQUEST = 1)
    const uint8_t *dhcp = &captured[14U + 20U + 8U];
    TEST_ASSERT_EQUAL_UINT8(1U, dhcp[0]);

    // Check message type option (53)
    // Options start at offset 240 of the DHCP payload
    uint32_t opt_idx = 240U;
    uint8_t msg_type = 0U;
    uint32_t dhcp_payload_limit = cap_len - (14U + 20U + 8U);
    while (opt_idx < dhcp_payload_limit)
    {
        uint8_t opt_code = dhcp[opt_idx];
        if (opt_code == 255U)
        {
            break;
        }
        if (opt_code == 0U)
        {
            opt_idx++;
            continue;
        }
        if (opt_idx + 1U >= dhcp_payload_limit)
        {
            break;
        }
        uint8_t opt_len = dhcp[opt_idx + 1U];
        if (opt_idx + 2U + opt_len > dhcp_payload_limit)
        {
            break;
        }
        if (opt_code == 53U)
        {
            msg_type = dhcp[opt_idx + 2U];
            break;
        }
        opt_idx += 2U + opt_len;
    }
    TEST_ASSERT_EQUAL_UINT8(expected_msg_type, msg_type);
}

static void inject_dhcp_reply(xNET_Context_t *net_ctx,
                              xNET_Interface_Context_t *interface_ctx,
                              uint8_t msg_type,
                              uint32_t xid,
                              const xNET_IPv4_Address_t *offered_ip,
                              const xNET_IPv4_Address_t *server_ip,
                              const xNET_IPv4_Address_t *netmask,
                              const xNET_IPv4_Address_t *gateway,
                              const xNET_IPv4_Address_t *dns_primary,
                              uint32_t lease_time)
{
    xNET_Packet_Buffer_t *pkt = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Packet_Alloc(net_ctx, &pkt));
    pkt->data_offset = 32U;

    // Calculate total size:
    // UDP Header (8 bytes) + DHCP payload (at least 240 bytes for header + cookie + options + end)
    // Let's make the DHCP payload 300 bytes. So total length = 308 bytes.
    uint32_t udp_payload_len = 300U;
    uint32_t total_len = 8U + udp_payload_len;
    pkt->data_length = total_len;

    uint8_t *udp_hdr = xNET_Packet_Get_Data(pkt);
    (void)memset(udp_hdr, 0, total_len);

    // 1. UDP Header
    // Source Port 67 (0x0043)
    udp_hdr[0] = 0x00U;
    udp_hdr[1] = 0x43U;
    // Dest Port 68 (0x0044)
    udp_hdr[2] = 0x00U;
    udp_hdr[3] = 0x44U;
    // Length (308 bytes)
    udp_hdr[4] = (uint8_t)((total_len >> 8U) & 0xFFU);
    udp_hdr[5] = (uint8_t)(total_len & 0xFFU);
    // Checksum = 0
    udp_hdr[6] = 0x00U;
    udp_hdr[7] = 0x00U;

    // 2. DHCP Payload
    uint8_t *dhcp = &udp_hdr[8];
    dhcp[0] = 2U; // BOOTREPLY
    dhcp[1] = 1U; // HTYPE ethernet
    dhcp[2] = 6U; // HLEN
    dhcp[3] = 0U; // hops

    // xid
    uint32_t xid_be = xNET_HTONL(xid);
    (void)memcpy(&dhcp[4], &xid_be, 4U);

    // yiaddr (offered_ip)
    if (offered_ip != NULL)
    {
        (void)memcpy(&dhcp[16], offered_ip->addr, 4U);
    }

    // chaddr
    (void)memcpy(&dhcp[28], interface_ctx->mac_addr.addr, 6U);

    // magic cookie
    uint32_t cookie_be = xNET_HTONL(0x63825363U);
    (void)memcpy(&dhcp[236], &cookie_be, 4U);

    // Options
    uint32_t opt = 240U;

    // Option 53: Message Type (length 1)
    dhcp[opt] = 53U;
    dhcp[opt + 1U] = 1U;
    dhcp[opt + 2U] = msg_type;
    opt += 3U;

    // Option 54: Server Identifier (length 4)
    if (server_ip != NULL)
    {
        dhcp[opt] = 54U;
        dhcp[opt + 1U] = 4U;
        (void)memcpy(&dhcp[opt + 2U], server_ip->addr, 4U);
        opt += 6U;
    }

    // Option 1: Subnet Mask (length 4)
    if (netmask != NULL)
    {
        dhcp[opt] = 1U;
        dhcp[opt + 1U] = 4U;
        (void)memcpy(&dhcp[opt + 2U], netmask->addr, 4U);
        opt += 6U;
    }

    // Option 3: Router/Gateway (length 4)
    if (gateway != NULL)
    {
        dhcp[opt] = 3U;
        dhcp[opt + 1U] = 4U;
        (void)memcpy(&dhcp[opt + 2U], gateway->addr, 4U);
        opt += 6U;
    }

    // Option 6: DNS Server (length 4)
    if (dns_primary != NULL)
    {
        dhcp[opt] = 6U;
        dhcp[opt + 1U] = 4U;
        (void)memcpy(&dhcp[opt + 2U], dns_primary->addr, 4U);
        opt += 6U;
    }

    // Option 51: Lease Time (length 4)
    if (lease_time != 0U)
    {
        dhcp[opt] = 51U;
        dhcp[opt + 1U] = 4U;
        dhcp[opt + 2U] = (uint8_t)((lease_time >> 24U) & 0xFFU);
        dhcp[opt + 3U] = (uint8_t)((lease_time >> 16U) & 0xFFU);
        dhcp[opt + 4U] = (uint8_t)((lease_time >> 8U) & 0xFFU);
        dhcp[opt + 5U] = (uint8_t)(lease_time & 0xFFU);
        opt += 6U;
    }

    // Option 255: End
    dhcp[opt] = 255U;

    // Deliver
    xNET_IPv4_Address_t src_ip;
    if (server_ip != NULL)
    {
        src_ip = *server_ip;
    }
    else
    {
        src_ip = (xNET_IPv4_Address_t){{0, 0, 0, 0}};
    }
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_UDP_RX(interface_ctx, pkt, &src_ip));
}

void test_xnet_dhcp_state_transitions(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // Initially, DHCP context is inactive, state is init (0)
    TEST_ASSERT_FALSE(interface_ctx.dhcp_ctx.is_active);

    // 1. Start DHCP
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_DHCP_Start(&interface_ctx));
    TEST_ASSERT_TRUE(interface_ctx.dhcp_ctx.is_active);
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_SELECTING, interface_ctx.dhcp_ctx.state);

    // Discover packet should have been sent to 255.255.255.255
    xNET_IPv4_Address_t broadcast_ip = {{255, 255, 255, 255}};
    verify_sent_dhcp_msg(&fake_ctx, 1U, &broadcast_ip); // 1 = DHCPDISCOVER

    // Get the transaction ID
    uint32_t xid = interface_ctx.dhcp_ctx.xid;

    // 2. Inject DHCPOFFER
    xNET_IPv4_Address_t offered_ip = {{192, 168, 1, 100}};
    xNET_IPv4_Address_t server_ip = {{192, 168, 1, 1}};
    xNET_IPv4_Address_t netmask = {{255, 255, 255, 0}};
    xNET_IPv4_Address_t gateway = {{192, 168, 1, 1}};
    xNET_IPv4_Address_t dns = {{8, 8, 8, 8}};

    inject_dhcp_reply(&net_ctx, &interface_ctx, 2U, xid, &offered_ip, &server_ip, &netmask, &gateway, &dns, 3600U); // 2 = DHCPOFFER

    // Should transition to REQUESTING
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_REQUESTING, interface_ctx.dhcp_ctx.state);

    // Request packet should have been sent
    verify_sent_dhcp_msg(&fake_ctx, 3U, &broadcast_ip); // 3 = DHCPREQUEST

    // 3. Inject DHCPACK
    inject_dhcp_reply(&net_ctx, &interface_ctx, 5U, xid, &offered_ip, &server_ip, &netmask, &gateway, &dns, 3600U); // 5 = DHCPACK

    // Should transition to BOUND
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_BOUND, interface_ctx.dhcp_ctx.state);

    // Interface IP configuration should now be set
    TEST_ASSERT_EQUAL_MEMORY(offered_ip.addr, interface_ctx.ip_addr.addr, 4);
    TEST_ASSERT_EQUAL_MEMORY(netmask.addr, interface_ctx.netmask.addr, 4);
    TEST_ASSERT_EQUAL_MEMORY(gateway.addr, interface_ctx.gateway.addr, 4);
    TEST_ASSERT_EQUAL_MEMORY(dns.addr, interface_ctx.dns_primary.addr, 4);

    // 4. Inject DHCPNAK to test fallback
    // Let's first move state to REQUESTING to simulate a request NAK
    interface_ctx.dhcp_ctx.state = xNET_DHCP_STATE_REQUESTING;
    inject_dhcp_reply(&net_ctx, &interface_ctx, 6U, xid, &offered_ip, &server_ip, &netmask, &gateway, &dns, 3600U); // 6 = DHCPNAK

    // Should transition to SELECTING (via INIT/Discover send)
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_SELECTING, interface_ctx.dhcp_ctx.state);
    // IP configuration should be cleared
    xNET_IPv4_Address_t zero_ip = {{0, 0, 0, 0}};
    TEST_ASSERT_EQUAL_MEMORY(zero_ip.addr, interface_ctx.ip_addr.addr, 4);

    // Stop DHCP
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_DHCP_Stop(&interface_ctx));
    TEST_ASSERT_FALSE(interface_ctx.dhcp_ctx.is_active);
}

void test_xnet_dhcp_lease_renewal_expiry(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // Start DHCP and transition to BOUND
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_DHCP_Start(&interface_ctx));
    uint32_t xid = interface_ctx.dhcp_ctx.xid;
    // Discover packet
    uint8_t captured[512];
    uint32_t cap_len;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    xNET_IPv4_Address_t offered_ip = {{192, 168, 1, 100}};
    xNET_IPv4_Address_t server_ip = {{192, 168, 1, 1}};
    xNET_IPv4_Address_t netmask = {{255, 255, 255, 0}};
    xNET_IPv4_Address_t gateway = {{192, 168, 1, 1}};

    // Offer
    inject_dhcp_reply(&net_ctx, &interface_ctx, 2U, xid, &offered_ip, &server_ip, &netmask, &gateway, NULL, 1000U); // 1000s lease
    // Request packet
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    // ACK
    inject_dhcp_reply(&net_ctx, &interface_ctx, 5U, xid, &offered_ip, &server_ip, &netmask, &gateway, NULL, 1000U);

    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_BOUND, interface_ctx.dhcp_ctx.state);
    TEST_ASSERT_EQUAL_UINT32(1000, interface_ctx.dhcp_ctx.lease_time);
    TEST_ASSERT_EQUAL_UINT32(500, interface_ctx.dhcp_ctx.t1);
    TEST_ASSERT_EQUAL_UINT32(875, interface_ctx.dhcp_ctx.t2);

    // Pre-populate ARP cache for server_ip so unicast sends succeed without sending ARP requests
    interface_ctx.arp_cache[0].ip_addr = server_ip;
    interface_ctx.arp_cache[0].mac_addr = (xNET_MAC_Address_t){{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;
    interface_ctx.arp_cache[0].timeout_ms = net_ctx.system_ticks + 10000000U;

    // 1. Tick up to T1 (500s) using xNET_Tick to verify full integration
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 499000U)); // 499 seconds
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_BOUND, interface_ctx.dhcp_ctx.state);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 2000U)); // Cross T1
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_RENEWING, interface_ctx.dhcp_ctx.state);

    // Request packet (unicast to server IP) should be sent
    verify_sent_dhcp_msg(&fake_ctx, 3U, &server_ip);

    // 2. Renewal succeeds: inject ACK
    inject_dhcp_reply(&net_ctx, &interface_ctx, 5U, xid, &offered_ip, &server_ip, &netmask, &gateway, NULL, 1000U);
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_BOUND, interface_ctx.dhcp_ctx.state);
    TEST_ASSERT_EQUAL_UINT32(0, interface_ctx.dhcp_ctx.t1_elapsed_ms);

    // Pre-populate ARP cache again because it was flushed by ACK
    interface_ctx.arp_cache[0].ip_addr = server_ip;
    interface_ctx.arp_cache[0].mac_addr = (xNET_MAC_Address_t){{0x00, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE}};
    interface_ctx.arp_cache[0].state = xNET_ARP_ENTRY_RESOLVED;
    interface_ctx.arp_cache[0].timeout_ms = net_ctx.system_ticks + 10000000U;

    // 3. Move to renewing again
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 501000U)); // Cross T1
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_RENEWING, interface_ctx.dhcp_ctx.state);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    // Tick until T2 (875s) to trigger REBINDING broadcast
    // Currently, t2_elapsed_ms is 501,000 ms. We need 875,000 ms.
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 374000U)); // 501 + 374 = 875s (T2 reached)
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_REBINDING, interface_ctx.dhcp_ctx.state);

    // Verify the unicast Request retry first
    verify_sent_dhcp_msg(&fake_ctx, 3U, &server_ip);

    // Verify the broadcast rebinding Request packet
    xNET_IPv4_Address_t broadcast_ip = {{255, 255, 255, 255}};
    verify_sent_dhcp_msg(&fake_ctx, 3U, &broadcast_ip);

    // 4. Tick until lease expiry (1000s)
    // Currently, lease_elapsed_total_ms is 875,000 ms. We need 1,000,000 ms.
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 125000U)); // 875 + 125 = 1000s (Lease expired)
    // Lease expires -> Fallback to SELECTING (via INIT/Discover)
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_SELECTING, interface_ctx.dhcp_ctx.state);

    // Verify the broadcast Request retry first
    verify_sent_dhcp_msg(&fake_ctx, 3U, &broadcast_ip);

    // Verify the broadcast Discover packet sent after lease expiry
    verify_sent_dhcp_msg(&fake_ctx, 1U, &broadcast_ip);

    // IP should be cleared
    xNET_IPv4_Address_t zero_ip = {{0, 0, 0, 0}};
    TEST_ASSERT_EQUAL_MEMORY(zero_ip.addr, interface_ctx.ip_addr.addr, 4);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_DHCP_Stop(&interface_ctx));
}

void test_xnet_dhcp_retries(void)
{
    xNET_Context_t net_ctx = {0};
    xNET_Config_t net_config = {0};
    static uint8_t pool[16384];
    net_config.packet_pool_buffer = pool;
    net_config.packet_pool_buffer_size = sizeof(pool);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Init(&net_ctx, &net_config));

    xNET_Interface_Context_t interface_ctx = {0};
    xNET_Fake_Interface_Context_t fake_ctx = {0};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_Init(&fake_ctx, &interface_ctx));
    interface_ctx.mac_addr = (xNET_MAC_Address_t){{0x00, 0x11, 0x22, 0x33, 0x44, 0x55}};
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Add(&net_ctx, &interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Interface_Link_Set(&interface_ctx, true));

    // Start DHCP
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_DHCP_Start(&interface_ctx));
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_SELECTING, interface_ctx.dhcp_ctx.state);

    // Initial Discover sent
    uint8_t captured[512];
    uint32_t cap_len;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));
    TEST_ASSERT_EQUAL_UINT32(0, interface_ctx.dhcp_ctx.retry_count);
    TEST_ASSERT_EQUAL_UINT32(4000, interface_ctx.dhcp_ctx.current_retry_timeout);

    // Retry 1: tick 4000ms
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 4000U));
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_SELECTING, interface_ctx.dhcp_ctx.state);
    TEST_ASSERT_EQUAL_UINT32(1, interface_ctx.dhcp_ctx.retry_count);
    TEST_ASSERT_EQUAL_UINT32(8000, interface_ctx.dhcp_ctx.current_retry_timeout);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    // Retry 2: tick 8000ms
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 8000U));
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_SELECTING, interface_ctx.dhcp_ctx.state);
    TEST_ASSERT_EQUAL_UINT32(2, interface_ctx.dhcp_ctx.retry_count);
    TEST_ASSERT_EQUAL_UINT32(16000, interface_ctx.dhcp_ctx.current_retry_timeout);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    // Retry 3: tick 16000ms
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 16000U));
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_SELECTING, interface_ctx.dhcp_ctx.state);
    TEST_ASSERT_EQUAL_UINT32(3, interface_ctx.dhcp_ctx.retry_count);
    TEST_ASSERT_EQUAL_UINT32(32000, interface_ctx.dhcp_ctx.current_retry_timeout);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    // Retry 4: tick 32000ms
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 32000U));
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_SELECTING, interface_ctx.dhcp_ctx.state);
    TEST_ASSERT_EQUAL_UINT32(4, interface_ctx.dhcp_ctx.retry_count);
    TEST_ASSERT_EQUAL_UINT32(64000, interface_ctx.dhcp_ctx.current_retry_timeout);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Fake_Interface_TX_Pop(&fake_ctx, captured, sizeof(captured), &cap_len));

    // Final tick to exhaust: tick 64000ms
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xNET_OK, xNET_Tick(&net_ctx, 64000U));
    // Should transition to FAILED and stop (inactive)
    TEST_ASSERT_EQUAL_UINT32(xNET_DHCP_STATE_FAILED, interface_ctx.dhcp_ctx.state);
    TEST_ASSERT_FALSE(interface_ctx.dhcp_ctx.is_active);
}

// PUBLIC FUNCTIONS IMPLEMENTATION /////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xnet_init_null_params);
    RUN_TEST(test_xnet_init_success);
    RUN_TEST(test_xnet_process_invalid_state);
    RUN_TEST(test_xnet_endian_swaps);
    RUN_TEST(test_xnet_checksum_simple);
    RUN_TEST(test_xnet_checksum_pseudo);
    RUN_TEST(test_xnet_interface_add_null_params);
    RUN_TEST(test_xnet_interface_add_success);
    RUN_TEST(test_xnet_interface_add_max_limit);
    RUN_TEST(test_xnet_interface_link_set);
    RUN_TEST(test_xnet_interface_process_poll);
    RUN_TEST(test_xnet_interface_rx_frame);
    RUN_TEST(test_xnet_packet_alloc_release_success);
    RUN_TEST(test_xnet_packet_alloc_limits);
    RUN_TEST(test_xnet_packet_helpers);
    RUN_TEST(test_xnet_packet_boundary_checks);
    RUN_TEST(test_xnet_fake_interface_lifecycle);
    RUN_TEST(test_xnet_fake_interface_tx_rx);
    RUN_TEST(test_xnet_fake_interface_limits);
    RUN_TEST(test_xnet_ethernet_helpers);
    RUN_TEST(test_xnet_ethernet_parse_build);
    RUN_TEST(test_xnet_ethernet_rx_filtering);
    RUN_TEST(test_xnet_ethernet_tx_padding);
    RUN_TEST(test_xnet_ethernet_routing);
    RUN_TEST(test_xnet_arp_cache_query);
    RUN_TEST(test_xnet_arp_resolve);
    RUN_TEST(test_xnet_arp_rx);
    RUN_TEST(test_xnet_arp_cache_eviction);
    RUN_TEST(test_xnet_arp_timeouts_retries);
    RUN_TEST(test_xnet_timer_wrap_safe_comparisons);
    RUN_TEST(test_xnet_timer_passive_tick);
    RUN_TEST(test_xnet_ipv4_rejects_malformed_headers);
    RUN_TEST(test_xnet_ipv4_rejects_fragments);
    RUN_TEST(test_xnet_ipv4_rejects_options);
    RUN_TEST(test_xnet_ipv4_rx_filtering_and_routing);
    RUN_TEST(test_xnet_ipv4_tx_building);
    RUN_TEST(test_xnet_icmp_rx_rejects_short_packet);
    RUN_TEST(test_xnet_icmp_rx_rejects_bad_checksum);
    RUN_TEST(test_xnet_icmp_rx_drops_unsupported_types);
    RUN_TEST(test_xnet_icmp_rx_echo_request_reply_in_place);
    RUN_TEST(test_xnet_icmp_rx_echo_request_hardware_offload);
    RUN_TEST(test_xnet_udp_open_close_bind);
    RUN_TEST(test_xnet_udp_rx_validation);
    RUN_TEST(test_xnet_udp_rx_delivery);
    RUN_TEST(test_xnet_udp_tx_send_to);
    RUN_TEST(test_xnet_udp_tx_rx_hardware_offload);
    RUN_TEST(test_xnet_ipv4_config_static_success);
    RUN_TEST(test_xnet_ipv4_config_static_validation);
    RUN_TEST(test_xnet_ipv4_config_static_null);
    RUN_TEST(test_xnet_dhcp_state_transitions);
    RUN_TEST(test_xnet_dhcp_lease_renewal_expiry);
    RUN_TEST(test_xnet_dhcp_retries);
    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
