// Copyright 2022 alambe94

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// @file test_win.c
// @brief Host tests for the xUSBD WinUSB vendor class driver.

#include <string.h>

#include "test_helpers.h"
#include "xusbd_win.h"
#include "xassert.h"

xSTATIC_ASSERT(xUSBD_WIN_DESC_SIZE(USB_SPEED_HIGH) <= xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
               "WinUSB descriptor exceeds config descriptor budget");
xSTATIC_ASSERT(xUSBD_WIN_DESC_SIZE(USB_SPEED_SUPER) <= xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
               "SuperSpeed WinUSB descriptor exceeds config descriptor budget");

// WinUSB app callback fakes ///////////////////////////////////////////////////

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

FAKE_VALUE_FUNC(xRETURN_t, fake_win_bus_event, xUSBD_Class_Context_t *, USB_DCD_Event_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_win_data_received, xUSBD_Class_Context_t *, uint8_t, uint8_t *, uint32_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_win_transmit_complete, xUSBD_Class_Context_t *, uint8_t, uint8_t *, uint32_t);

#pragma GCC diagnostic pop

#define RESET_WIN_FAKES()                                                                                                                  \
    do                                                                                                                                     \
    {                                                                                                                                      \
        RESET_FAKE(fake_win_bus_event);                                                                                                    \
        RESET_FAKE(fake_win_data_received);                                                                                                \
        RESET_FAKE(fake_win_transmit_complete);                                                                                            \
    } while (0)

// Endpoint addresses assigned after xUSBD_Init (next_out_ep=0x01, next_in_ep=0x81).
#define WIN_OUT_EP 0x01U
#define WIN_IN_EP  0x81U

// Test fixtures ///////////////////////////////////////////////////////////////

static xUSBD_Device_Context_t g_device;
static xUSBD_WIN_Context_t g_win;

static xUSBD_WIN_Callbacks_t g_win_calls = {
    .on_bus_event = fake_win_bus_event,
    .on_data_received = fake_win_data_received,
    .on_transmit_complete = fake_win_transmit_complete,
};

static void win_register(void)
{
    (void)xUSBD_Class_Register(&g_device, &g_win.class_ctx, xUSBD_WIN_Class());
}

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
    RESET_WIN_FAKES();
    memset(&g_device, 0, sizeof(g_device));
    memset(&g_win, 0, sizeof(g_win));
    test_device_init(&g_device);
}

void tearDown(void)
{
}

// REGISTRATION ////////////////////////////////////////////////////////////////

void test_win_register_succeeds(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Register(&g_device, &g_win.class_ctx, xUSBD_WIN_Class()));
}

void test_win_register_allocates_one_interface_one_in_ep_one_out_ep_one_string(void)
{
    uint8_t iface_before = g_device.next_interface;
    uint8_t in_ep_before = g_device.next_in_ep;
    uint8_t out_ep_before = g_device.next_out_ep;
    uint8_t str_before = g_device.next_string_index;

    win_register();

    TEST_ASSERT_EQUAL_UINT8(iface_before + 1U, g_device.next_interface);
    TEST_ASSERT_EQUAL_UINT8(in_ep_before + 1U, g_device.next_in_ep);
    TEST_ASSERT_EQUAL_UINT8(out_ep_before + 1U, g_device.next_out_ep);
    TEST_ASSERT_EQUAL_UINT8(str_before, g_device.next_string_index);
}

void test_win_register_after_start_fails(void)
{
    win_register();
    (void)test_device_start(&g_device);

    xUSBD_WIN_Context_t extra;
    memset(&extra, 0, sizeof(extra));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_ALREADY_INITIALIZED, xUSBD_Class_Register(&g_device, &extra.class_ctx, xUSBD_WIN_Class()));
}

// DESCRIPTOR //////////////////////////////////////////////////////////////////

void test_win_descriptor_size_high_speed(void)
{
    win_register();
    uint32_t size = xUSBD_WIN_DESC_SIZE(USB_SPEED_HIGH);
    TEST_ASSERT_EQUAL_UINT32(xUSBD_WIN_DESC_SIZE(USB_SPEED_HIGH), size);
}

void test_win_descriptor_size_super_speed(void)
{
    win_register();
    uint32_t size = xUSBD_WIN_DESC_SIZE(USB_SPEED_SUPER);
    TEST_ASSERT_EQUAL_UINT32(xUSBD_WIN_DESC_SIZE(USB_SPEED_SUPER), size);
}

void test_win_descriptor_build_matches_declared_size(void)
{
    win_register();
    uint32_t declared = xUSBD_WIN_DESC_SIZE(USB_SPEED_HIGH);
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    uint32_t built = xUSBD_WIN_Class()->build_descriptor(&g_win.class_ctx, buf, USB_SPEED_HIGH);
    TEST_ASSERT_EQUAL_UINT32(declared, built);
}

// APP CALLBACKS ///////////////////////////////////////////////////////////////

void test_win_set_callbacks_success(void)
{
    win_register();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls));
}

void test_win_set_callbacks_null_ctx_fails(void)
{
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xUSBD_WIN_Set_Callbacks((xUSBD_Class_Context_t *)NULL, &g_win_calls));
}

void test_win_set_callbacks_roundtrip(void)
{
    win_register();
    (void)xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls);
    void *retrieved = NULL;
    (void)xUSBD_Class_Get_Callbacks(&g_win.class_ctx, &retrieved);
    TEST_ASSERT_EQUAL_PTR(&g_win_calls, retrieved);
}

// BUS EVENTS //////////////////////////////////////////////////////////////////

void test_win_connect_fires_app_callback(void)
{
    win_register();
    (void)xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls);
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_win_bus_event_fake.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_CONNECT_RECEIVED, fake_win_bus_event_fake.arg1_val);
}

void test_win_connect_inits_bulk_endpoints(void)
{
    win_register();
    (void)xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls);
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    // 2 for EP0 (from dcd_connect_event_process) + 2 for WIN bulk endpoints (from win_bus_event)
    TEST_ASSERT_EQUAL_UINT32(4U, fake_dcd_ep_init_fake.call_count);
}

void test_win_disconnect_deinits_bulk_endpoints(void)
{
    win_register();
    (void)xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls);
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    RESET_FAKE(fake_dcd_ep_deinit);
    dcd_fire_event(&g_device, USB_DCD_DISCONNECT_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(2U, fake_dcd_ep_deinit_fake.call_count);
}

void test_win_reset_deinits_bulk_endpoints(void)
{
    win_register();
    (void)xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls);
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    RESET_FAKE(fake_dcd_ep_deinit);
    dcd_fire_event(&g_device, USB_DCD_RESET_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(2U, fake_dcd_ep_deinit_fake.call_count);
}

void test_win_speed_change_reinits_bulk_endpoints_with_super_speed_mps(void)
{
    uint16_t ep_mps = 0U;

    win_register();
    (void)xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls);
    (void)test_device_start(&g_device);
    fake_dcd_get_speed_fake.return_val = USB_SPEED_HIGH;
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    RESET_FAKE(fake_dcd_ep_init);
    RESET_FAKE(fake_dcd_ep_receive);
    RESET_FAKE(fake_win_bus_event);
    fake_dcd_get_speed_fake.return_val = USB_SPEED_SUPER;
    dcd_fire_event(&g_device, USB_DCD_SPEED_CHANGE_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL(USB_SPEED_SUPER, g_device.speed);
    TEST_ASSERT_EQUAL_UINT32(4U, fake_dcd_ep_init_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_receive_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(WIN_OUT_EP, fake_dcd_ep_init_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT16(1024U, fake_dcd_ep_init_fake.arg3_val);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_win_bus_event_fake.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_SPEED_CHANGE_RECEIVED, fake_win_bus_event_fake.arg1_val);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_EP_MPS(&g_win.class_ctx, &ep_mps));
    TEST_ASSERT_EQUAL_UINT16(1024U, ep_mps);
}

void test_win_no_callbacks_does_not_crash(void)
{
    win_register();
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);
}

void test_win_bus_event_callback_error_does_not_abort_endpoint_init(void)
{
    win_register();
    (void)xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls);
    (void)test_device_start(&g_device);

    fake_win_bus_event_fake.return_val = xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    RESET_FAKE(fake_dcd_ep_init);

    xRETURN_t status = xUSBD_WIN_Class()->bus_event(&g_win.class_ctx, USB_DCD_CONNECT_RECEIVED);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_INVALID_CLASS_REQ, status);
    TEST_ASSERT_EQUAL_UINT32(2U, fake_dcd_ep_init_fake.call_count);
}

// DATA PATH ///////////////////////////////////////////////////////////////////

void test_win_transmit_calls_ep_send(void)
{
    win_register();
    (void)test_device_start(&g_device);
    // build_descriptor (called during start) sets ep_mps so Transmit can compute ZLP flag
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    uint8_t buf[4] = {0x01, 0x02, 0x03, 0x04};
    RESET_FAKE(fake_dcd_ep_send);
    (void)xUSBD_WIN_Transmit(&g_win.class_ctx, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(WIN_IN_EP, fake_dcd_ep_send_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(sizeof(buf), fake_dcd_ep_send_fake.arg3_val);
}

void test_win_transmit_zlp_on_full_packet(void)
{
    win_register();
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    // ep_mps for HS bulk = 512. Send exactly 512 bytes -> ZLP required.
    static uint8_t buf[512];
    memset(buf, 0xAAU, sizeof(buf));

    RESET_FAKE(fake_dcd_ep_send);
    (void)xUSBD_WIN_Transmit(&g_win.class_ctx, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    // arg4 = send_zlp; should be non-zero (true)
    TEST_ASSERT_NOT_EQUAL(0U, (uint32_t)fake_dcd_ep_send_fake.arg4_val);
}

void test_win_prepare_to_receive_calls_ep_receive(void)
{
    win_register();
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    uint8_t buf[64];
    RESET_FAKE(fake_dcd_ep_receive);
    (void)xUSBD_WIN_Prepare_To_Receive(&g_win.class_ctx, buf, sizeof(buf));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_receive_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(WIN_OUT_EP, fake_dcd_ep_receive_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(sizeof(buf), fake_dcd_ep_receive_fake.arg3_val);
}

void test_win_data_received_fires_app_callback(void)
{
    win_register();
    (void)xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls);
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    uint8_t incoming[8] = {0xDE, 0xAD, 0xBE, 0xEF, 0, 0, 0, 0};
    dcd_fire_event(&g_device, USB_DCD_DATA_RECEIVED, WIN_OUT_EP, incoming, sizeof(incoming));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_win_data_received_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(WIN_OUT_EP, fake_win_data_received_fake.arg1_val);
    TEST_ASSERT_EQUAL_PTR(incoming, fake_win_data_received_fake.arg2_val);
    TEST_ASSERT_EQUAL_UINT32(sizeof(incoming), fake_win_data_received_fake.arg3_val);
}

void test_win_transmit_complete_fires_app_callback(void)
{
    win_register();
    (void)xUSBD_WIN_Set_Callbacks(&g_win.class_ctx, &g_win_calls);
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    dcd_fire_event(&g_device, USB_DCD_DATA_SENT, WIN_IN_EP, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_win_transmit_complete_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(WIN_IN_EP, fake_win_transmit_complete_fake.arg1_val);
}

// RUNNER //////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_win_register_succeeds);
    RUN_TEST(test_win_register_allocates_one_interface_one_in_ep_one_out_ep_one_string);
    RUN_TEST(test_win_register_after_start_fails);

    RUN_TEST(test_win_descriptor_size_high_speed);
    RUN_TEST(test_win_descriptor_size_super_speed);
    RUN_TEST(test_win_descriptor_build_matches_declared_size);

    RUN_TEST(test_win_set_callbacks_success);
    RUN_TEST(test_win_set_callbacks_null_ctx_fails);
    RUN_TEST(test_win_set_callbacks_roundtrip);

    RUN_TEST(test_win_connect_fires_app_callback);
    RUN_TEST(test_win_connect_inits_bulk_endpoints);
    RUN_TEST(test_win_disconnect_deinits_bulk_endpoints);
    RUN_TEST(test_win_reset_deinits_bulk_endpoints);
    RUN_TEST(test_win_speed_change_reinits_bulk_endpoints_with_super_speed_mps);
    RUN_TEST(test_win_no_callbacks_does_not_crash);
    RUN_TEST(test_win_bus_event_callback_error_does_not_abort_endpoint_init);

    RUN_TEST(test_win_transmit_calls_ep_send);
    RUN_TEST(test_win_transmit_zlp_on_full_packet);
    RUN_TEST(test_win_prepare_to_receive_calls_ep_receive);
    RUN_TEST(test_win_data_received_fires_app_callback);
    RUN_TEST(test_win_transmit_complete_fires_app_callback);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
