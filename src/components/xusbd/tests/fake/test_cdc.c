// Copyright 2022 alambe94
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

// @file test_cdc.c
// @brief Host tests for the xUSBD CDC (Communications Device Class) class driver.

#include <string.h>

#include "test_helpers.h"
#include "xusbd_cdc.h"
#include "xassert.h"

xSTATIC_ASSERT(xUSBD_CDC_DESC_SIZE(USB_SPEED_HIGH, 0U) <= xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
               "CDC descriptor exceeds config descriptor budget");
xSTATIC_ASSERT(xUSBD_CDC_DESC_SIZE(USB_SPEED_SUPER, 0U) <= xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
               "SuperSpeed CDC descriptor exceeds config descriptor budget");

// CDC app callback fakes //////////////////////////////////////////////////////

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

FAKE_VALUE_FUNC(xRETURN_t, fake_cdc_bus_event, xUSBD_Class_Context_t *, USB_DCD_Event_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_cdc_control_in, xUSBD_Class_Context_t *, xUSBD_Response_t *);
FAKE_VALUE_FUNC(xRETURN_t, fake_cdc_control_out, xUSBD_Class_Context_t *, uint8_t *, uint32_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_cdc_data_received, xUSBD_Class_Context_t *, uint8_t, uint8_t *, uint32_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_cdc_transmit_complete, xUSBD_Class_Context_t *, uint8_t, uint8_t *, uint32_t);

#pragma GCC diagnostic pop

#define RESET_CDC_FAKES()                                                                                                                  \
    do                                                                                                                                     \
    {                                                                                                                                      \
        RESET_FAKE(fake_cdc_bus_event);                                                                                                    \
        RESET_FAKE(fake_cdc_control_in);                                                                                                   \
        RESET_FAKE(fake_cdc_control_out);                                                                                                  \
        RESET_FAKE(fake_cdc_data_received);                                                                                                \
        RESET_FAKE(fake_cdc_transmit_complete);                                                                                            \
    } while (0)

// Test fixtures ///////////////////////////////////////////////////////////////

static xUSBD_Device_Context_t g_device;
static xUSBD_CDC_Context_t g_cdc;

static xUSBD_CDC_Callbacks_t g_cdc_calls = {
    .on_bus_event = fake_cdc_bus_event,
    .on_control_in = fake_cdc_control_in,
    .on_control_out = fake_cdc_control_out,
    .on_data_received = fake_cdc_data_received,
    .on_transmit_complete = fake_cdc_transmit_complete,
};

static void cdc_register(void)
{
    (void)xUSBD_Class_Register(&g_device, &g_cdc.class_ctx, xUSBD_CDC_Class());
}

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
    RESET_CDC_FAKES();
    memset(&g_device, 0, sizeof(g_device));
    memset(&g_cdc, 0, sizeof(g_cdc));
    g_cdc.subclass = 0x02U; // ACM
    g_cdc.protocol = 0x01U; // ACM
    g_cdc.cmd_ep_interval = 0x08U;
    g_cdc.cmd_ep_mps = 64U;
    g_cdc.has_notification_ep = true;
    test_device_init(&g_device);
}

void tearDown(void)
{
}

// REGISTRATION ////////////////////////////////////////////////////////////////

void test_cdc_register_succeeds(void)
{
    xRETURN_t status = xUSBD_Class_Register(&g_device, &g_cdc.class_ctx, xUSBD_CDC_Class());
    TEST_ASSERT_EQUAL(xRETURN_OK, status);
}

void test_cdc_register_allocates_two_interfaces(void)
{
    uint8_t before = g_device.next_interface;
    cdc_register();
    TEST_ASSERT_EQUAL_UINT8(before + 2U, g_device.next_interface);
}

void test_cdc_register_allocates_three_endpoints(void)
{
    uint8_t in_before = g_device.next_in_ep;
    uint8_t out_before = g_device.next_out_ep;
    cdc_register();
    // cmd_ep (IN) + in_ep (IN) = 2 IN endpoints; out_ep (OUT) = 1 OUT endpoint
    TEST_ASSERT_EQUAL_UINT8(in_before + 2U, g_device.next_in_ep);
    TEST_ASSERT_EQUAL_UINT8(out_before + 1U, g_device.next_out_ep);
}

void test_cdc_register_allocates_no_string(void)
{
    uint8_t before = g_device.next_string_index;
    cdc_register();
    TEST_ASSERT_EQUAL_UINT8(before, g_device.next_string_index);
}

void test_cdc_set_interface_string_allocates_one_string(void)
{
    cdc_register();
    uint8_t before = g_device.next_string_index;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_Interface_String(&g_cdc.class_ctx, "CDC Test"));
    TEST_ASSERT_EQUAL_UINT8(before + 1U, g_device.next_string_index);
    TEST_ASSERT_EQUAL_UINT8(before, g_cdc.class_ctx.interface_string_index);
}

void test_cdc_register_after_start_fails(void)
{
    cdc_register();
    (void)test_device_start(&g_device);

    xUSBD_CDC_Context_t extra;
    memset(&extra, 0, sizeof(extra));
    xRETURN_t status = xUSBD_Class_Register(&g_device, &extra.class_ctx, xUSBD_CDC_Class());
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_ALREADY_INITIALIZED, status);
}

// DESCRIPTOR //////////////////////////////////////////////////////////////////

void test_cdc_descriptor_size_is_nonzero(void)
{
    cdc_register();
    uint32_t size = xUSBD_CDC_DESC_SIZE(USB_SPEED_HIGH, 19U);
    TEST_ASSERT_EQUAL_UINT32(xUSBD_CDC_DESC_SIZE(USB_SPEED_HIGH, 19U), size);
}

void test_cdc_descriptor_size_super_speed(void)
{
    cdc_register();
    uint32_t size = xUSBD_CDC_DESC_SIZE(USB_SPEED_SUPER, 19U);
    TEST_ASSERT_EQUAL_UINT32(xUSBD_CDC_DESC_SIZE(USB_SPEED_SUPER, 19U), size);
}

void test_cdc_descriptor_build_matches_declared_size(void)
{
    cdc_register();
    uint32_t declared = xUSBD_CDC_DESC_SIZE(USB_SPEED_HIGH, 19U);
    uint8_t buf[256];
    memset(buf, 0, sizeof(buf));
    uint32_t built = xUSBD_CDC_Class()->build_descriptor(&g_cdc.class_ctx, buf, USB_SPEED_HIGH);
    TEST_ASSERT_EQUAL_UINT32(declared, built);
}

void test_cdc_descriptor_build_has_communication_class(void)
{
    cdc_register();
    uint8_t buf[256];
    memset(buf, 0, sizeof(buf));
    (void)xUSBD_CDC_Class()->build_descriptor(&g_cdc.class_ctx, buf, USB_SPEED_HIGH);
    // IAD bFunctionClass is at offset 4 of the IAD descriptor (8 bytes)
    // buf[0..7] = IAD; buf[4] = bFunctionClass
    TEST_ASSERT_EQUAL_UINT8(USB_CLASS_COMMUNICATION, buf[4]);
}

// APP CALLBACKS ///////////////////////////////////////////////////////////////

void test_cdc_set_callbacks_success(void)
{
    cdc_register();
    xRETURN_t status = xUSBD_CDC_Set_Callbacks(&g_cdc.class_ctx, &g_cdc_calls);
    TEST_ASSERT_EQUAL(xRETURN_OK, status);
}

void test_cdc_set_callbacks_null_ctx_fails(void)
{
    xRETURN_t status = xUSBD_CDC_Set_Callbacks((xUSBD_Class_Context_t *)NULL, &g_cdc_calls);
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, status);
}

void test_cdc_set_callbacks_roundtrip(void)
{
    cdc_register();
    (void)xUSBD_CDC_Set_Callbacks(&g_cdc.class_ctx, &g_cdc_calls);
    void *retrieved = NULL;
    (void)xUSBD_Class_Get_Callbacks(&g_cdc.class_ctx, &retrieved);
    TEST_ASSERT_EQUAL_PTR(&g_cdc_calls, retrieved);
}

// BUS EVENTS //////////////////////////////////////////////////////////////////

void test_cdc_bus_event_connect_fires_app_callback(void)
{
    cdc_register();
    (void)xUSBD_CDC_Set_Callbacks(&g_cdc.class_ctx, &g_cdc_calls);
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_cdc_bus_event_fake.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_CONNECT_RECEIVED, fake_cdc_bus_event_fake.arg1_val);
}

void test_cdc_bus_event_disconnect_fires_app_callback(void)
{
    cdc_register();
    (void)xUSBD_CDC_Set_Callbacks(&g_cdc.class_ctx, &g_cdc_calls);
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);
    RESET_CDC_FAKES();

    dcd_fire_event(&g_device, USB_DCD_DISCONNECT_RECEIVED, 0, NULL, 0);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_cdc_bus_event_fake.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_DISCONNECT_RECEIVED, fake_cdc_bus_event_fake.arg1_val);
}

void test_cdc_bus_event_no_callbacks_does_not_crash(void)
{
    cdc_register();
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);
}

void test_cdc_bus_event_callback_error_does_not_abort_endpoint_init(void)
{
    cdc_register();
    (void)xUSBD_CDC_Set_Callbacks(&g_cdc.class_ctx, &g_cdc_calls);
    (void)test_device_start(&g_device);

    fake_cdc_bus_event_fake.return_val = xRETURN_xERR_xUSBD_INVALID_CLASS_REQ;
    RESET_FAKE(fake_dcd_ep_init);

    xRETURN_t status = xUSBD_CDC_Class()->bus_event(&g_cdc.class_ctx, USB_DCD_CONNECT_RECEIVED);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_INVALID_CLASS_REQ, status);
    TEST_ASSERT_EQUAL_UINT32(3U, fake_dcd_ep_init_fake.call_count);
}

void test_cdc_control_in_forwards_to_callback(void)
{
    cdc_register();
    (void)xUSBD_CDC_Set_Callbacks(&g_cdc.class_ctx, &g_cdc_calls);

    USB_Setup_Request_t req = {0};
    xUSBD_Response_t response = {0};

    fake_cdc_control_in_fake.return_val = xRETURN_OK;
    g_device.request = req;
    xRETURN_t status = xUSBD_CDC_Class()->control_in_request(&g_cdc.class_ctx, &response);

    TEST_ASSERT_EQUAL(xRETURN_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_cdc_control_in_fake.call_count);
}

void test_cdc_control_out_forwards_to_callback(void)
{
    cdc_register();
    (void)xUSBD_CDC_Set_Callbacks(&g_cdc.class_ctx, &g_cdc_calls);

    USB_Setup_Request_t req = {0};
    uint8_t data[8] = {0};

    fake_cdc_control_out_fake.return_val = xRETURN_OK;
    g_device.request = req;
    xRETURN_t status = xUSBD_CDC_Class()->control_out_request(&g_cdc.class_ctx, data, sizeof(data));

    TEST_ASSERT_EQUAL(xRETURN_OK, status);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_cdc_control_out_fake.call_count);
}

// RUNNER //////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_cdc_register_succeeds);
    RUN_TEST(test_cdc_register_allocates_two_interfaces);
    RUN_TEST(test_cdc_register_allocates_three_endpoints);
    RUN_TEST(test_cdc_register_allocates_no_string);
    RUN_TEST(test_cdc_set_interface_string_allocates_one_string);
    RUN_TEST(test_cdc_register_after_start_fails);

    RUN_TEST(test_cdc_descriptor_size_is_nonzero);
    RUN_TEST(test_cdc_descriptor_size_super_speed);
    RUN_TEST(test_cdc_descriptor_build_matches_declared_size);
    RUN_TEST(test_cdc_descriptor_build_has_communication_class);

    RUN_TEST(test_cdc_set_callbacks_success);
    RUN_TEST(test_cdc_set_callbacks_null_ctx_fails);
    RUN_TEST(test_cdc_set_callbacks_roundtrip);

    RUN_TEST(test_cdc_bus_event_connect_fires_app_callback);
    RUN_TEST(test_cdc_bus_event_disconnect_fires_app_callback);
    RUN_TEST(test_cdc_bus_event_no_callbacks_does_not_crash);
    RUN_TEST(test_cdc_bus_event_callback_error_does_not_abort_endpoint_init);

    RUN_TEST(test_cdc_control_in_forwards_to_callback);
    RUN_TEST(test_cdc_control_out_forwards_to_callback);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
