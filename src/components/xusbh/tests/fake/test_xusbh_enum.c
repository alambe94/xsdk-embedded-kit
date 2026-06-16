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

// @file test_xusbh_enum.c
// @brief Host tests for xUSBH direct-attached-device enumeration.

#include <string.h>

#include "unity.h"

#include "xusb_setup.h"
#include "xusbh_core.h"
#include "test_xusbh_helpers.h"

static xUSBH_Context_t g_host;

static const uint8_t valid_device_descriptor[USB_DEVICE_DESC_LEN] = {
    USB_DEVICE_DESC_LEN,
    USB_DESC_TYPE_DEVICE,
    0x00U,
    0x02U,
    0x00U,
    0x00U,
    0x00U,
    64U,
    0x34U,
    0x12U,
    0x78U,
    0x56U,
    0x00U,
    0x01U,
    1U,
    2U,
    3U,
    1U,
};

static const uint8_t valid_config_descriptor[] = {
    USB_CONFIGURATION_DESC_LEN,
    USB_DESC_TYPE_CONFIGURATION,
    25U,
    0U,
    1U,
    2U,
    0U,
    0x80U,
    50U,
    USB_INTERFACE_DESC_LEN,
    USB_DESC_TYPE_INTERFACE,
    0U,
    0U,
    1U,
    USB_CLASS_HID,
    1U,
    1U,
    0U,
    USB_ENDPOINT_DESC_LEN,
    USB_DESC_TYPE_ENDPOINT,
    0x81U,
    USB_ENDP_TYPE_INTR,
    64U,
    0U,
    8U,
};

static const uint8_t duplicate_endpoint_config_descriptor[] = {
    USB_CONFIGURATION_DESC_LEN,
    USB_DESC_TYPE_CONFIGURATION,
    32U,
    0U,
    1U,
    2U,
    0U,
    0x80U,
    50U,
    USB_INTERFACE_DESC_LEN,
    USB_DESC_TYPE_INTERFACE,
    0U,
    0U,
    2U,
    USB_CLASS_HID,
    1U,
    1U,
    0U,
    USB_ENDPOINT_DESC_LEN,
    USB_DESC_TYPE_ENDPOINT,
    0x81U,
    USB_ENDP_TYPE_INTR,
    64U,
    0U,
    8U,
    USB_ENDPOINT_DESC_LEN,
    USB_DESC_TYPE_ENDPOINT,
    0x81U,
    USB_ENDP_TYPE_INTR,
    64U,
    0U,
    8U,
};

static void emit_port_event(xUSBH_HCD_Port_Event_t port_event)
{
    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_PORT,
        .port = 0U,
        .port_event = port_event,
    };

    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &event);
}

static void complete_current_transfer(const uint8_t *data, uint32_t length)
{
    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_TRANSFER,
        .transfer_event = xUSBH_HCD_TRANSFER_EVENT_COMPLETE,
        .transfer = g_fake_hcd.last_transfer,
    };

    if ((data != NULL) && (length > 0U))
    {
        (void)memcpy(g_fake_hcd.last_transfer->data, data, length);
    }
    g_fake_hcd.last_transfer->actual_length = length;
    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &event);
}

static void drive_to_enumeration_start(void)
{
    emit_port_event(xUSBH_HCD_PORT_EVENT_CONNECTED);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    emit_port_event(xUSBH_HCD_PORT_EVENT_RESET_COMPLETE);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ENUMERATING, g_host.root_ports[0].state);
    TEST_ASSERT_EQUAL(xUSBH_ENUMERATION_GET_DEVICE_HEADER_SUBMIT, g_host.enumeration.state);
}

static void expect_setup(uint8_t request, uint8_t descriptor_type, uint16_t length)
{
    TEST_ASSERT_NOT_NULL(g_fake_hcd.last_transfer);
    TEST_ASSERT_TRUE(g_fake_hcd.last_transfer->has_setup);
    TEST_ASSERT_EQUAL_UINT8(request, g_fake_hcd.last_transfer->setup.bRequest);
    TEST_ASSERT_EQUAL_UINT16(length, xUSB_Setup_Get_Length(&g_fake_hcd.last_transfer->setup));
    if (request == USB_REQ_GET_DESCRIPTOR)
    {
        TEST_ASSERT_EQUAL_UINT8(descriptor_type, xUSB_Setup_Get_Descriptor_Type(&g_fake_hcd.last_transfer->setup));
    }
}

static void drive_to_config_full_request(const uint8_t *config_descriptor, uint16_t config_length)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    complete_current_transfer(valid_device_descriptor, 8U);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    complete_current_transfer(NULL, 0U);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    complete_current_transfer(valid_device_descriptor, USB_DEVICE_DESC_LEN);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    complete_current_transfer(config_descriptor, USB_CONFIGURATION_DESC_LEN);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    expect_setup(USB_REQ_GET_DESCRIPTOR, USB_DESC_TYPE_CONFIGURATION, config_length);
}

void setUp(void)
{
    (void)memset(&g_host, 0, sizeof(g_host));
    reset_fake_hcd();
}

void tearDown(void)
{
}

void test_enumeration_success_runs_control_flow_to_configured(void)
{
    xUSBH_Endpoint_Context_t *endpoint = NULL;

    init_and_start_host(&g_host);
    drive_to_enumeration_start();

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    expect_setup(USB_REQ_GET_DESCRIPTOR, USB_DESC_TYPE_DEVICE, 8U);
    complete_current_transfer(valid_device_descriptor, 8U);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL_UINT16(64U, g_host.devices[0].ep0_max_packet_size);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    expect_setup(USB_REQ_SET_ADDRESS, 0U, 0U);
    TEST_ASSERT_EQUAL_UINT16(1U, xUSB_Setup_Get_Value(&g_fake_hcd.last_transfer->setup));
    complete_current_transfer(NULL, 0U);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL_UINT8(1U, g_host.devices[0].address);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    expect_setup(USB_REQ_GET_DESCRIPTOR, USB_DESC_TYPE_DEVICE, USB_DEVICE_DESC_LEN);
    TEST_ASSERT_EQUAL_UINT8(1U, g_fake_hcd.last_transfer->device_address);
    complete_current_transfer(valid_device_descriptor, USB_DEVICE_DESC_LEN);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL_UINT16(0x1234U, g_host.devices[0].vendor_id);
    TEST_ASSERT_EQUAL_UINT16(0x5678U, g_host.devices[0].product_id);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    expect_setup(USB_REQ_GET_DESCRIPTOR, USB_DESC_TYPE_CONFIGURATION, USB_CONFIGURATION_DESC_LEN);
    complete_current_transfer(valid_config_descriptor, USB_CONFIGURATION_DESC_LEN);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL_UINT16(sizeof(valid_config_descriptor), g_host.enumeration.config_total_length);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    expect_setup(USB_REQ_GET_DESCRIPTOR, USB_DESC_TYPE_CONFIGURATION, sizeof(valid_config_descriptor));
    complete_current_transfer(valid_config_descriptor, sizeof(valid_config_descriptor));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_TRUE(g_host.interfaces[0].is_allocated);
    TEST_ASSERT_EQUAL_UINT8(0U, g_host.interfaces[0].device_index);
    TEST_ASSERT_EQUAL_UINT8(0U, g_host.interfaces[0].interface_number);
    TEST_ASSERT_EQUAL_UINT8(0U, g_host.interfaces[0].alternate_setting);
    TEST_ASSERT_EQUAL_UINT8(USB_CLASS_HID, g_host.interfaces[0].class_code);
    TEST_ASSERT_EQUAL_UINT8(1U, g_host.interfaces[0].subclass);
    TEST_ASSERT_EQUAL_UINT8(1U, g_host.interfaces[0].protocol);
    TEST_ASSERT_EQUAL_UINT8(1U, g_host.interfaces[0].endpoint_count);
    TEST_ASSERT_TRUE(g_host.endpoints[0].is_allocated);
    TEST_ASSERT_EQUAL_UINT8(0U, g_host.endpoints[0].device_index);
    TEST_ASSERT_EQUAL_UINT8(0U, g_host.endpoints[0].interface_index);
    TEST_ASSERT_EQUAL_UINT8(0x81U, g_host.endpoints[0].endpoint_address);
    TEST_ASSERT_EQUAL_UINT8(USB_ENDP_TYPE_INTR, g_host.endpoints[0].endpoint_type);
    TEST_ASSERT_TRUE(g_host.endpoints[0].is_in);
    TEST_ASSERT_EQUAL_UINT16(64U, g_host.endpoints[0].max_packet_size);
    TEST_ASSERT_EQUAL_UINT8(8U, g_host.endpoints[0].interval);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Find_By_Address(&g_host, &g_host.devices[0], 0x81U, &endpoint));
    TEST_ASSERT_EQUAL_PTR(&g_host.endpoints[0], endpoint);
    endpoint = NULL;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_OBJECT, xUSBH_Endpoint_Find_By_Address(&g_host, &g_host.devices[0], 0x02U, &endpoint));
    TEST_ASSERT_NULL(endpoint);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    expect_setup(USB_REQ_SET_CONFIGURATION, 0U, 0U);
    TEST_ASSERT_EQUAL_UINT16(2U, xUSB_Setup_Get_Value(&g_fake_hcd.last_transfer->setup));
    complete_current_transfer(NULL, 0U);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_CONFIGURED, g_host.root_ports[0].state);
    TEST_ASSERT_EQUAL(xUSBH_ENUMERATION_COMPLETE, g_host.enumeration.state);
    TEST_ASSERT_TRUE(g_host.devices[0].is_configured);
    TEST_ASSERT_EQUAL(xUSBH_DEVICE_STATE_CONFIGURED, g_host.devices[0].state);
    TEST_ASSERT_EQUAL_UINT8(2U, g_host.devices[0].active_configuration_value);
    TEST_ASSERT_FALSE(g_host.transfers[0].is_allocated);
}

void test_enumeration_times_out_pending_control_transfer(void)
{
    uint16_t i;

    init_and_start_host(&g_host);
    drive_to_enumeration_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));

    for (i = 0U; i < xUSBH_CONTROL_TRANSFER_TIMEOUT_TICKS; i++)
    {
        TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    }

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_TIMEOUT, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ERROR, g_host.root_ports[0].state);
    TEST_ASSERT_EQUAL(xUSBH_ENUMERATION_ERROR, g_host.enumeration.state);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.cancel_transfer_count);
}

void test_enumeration_rejects_invalid_ep0_mps_for_speed(void)
{
    uint8_t bad_device_header[8] = {
        USB_DEVICE_DESC_LEN, USB_DESC_TYPE_DEVICE, 0U, 2U, 0U, 0U, 0U, 8U,
    };

    init_and_start_host(&g_host);
    drive_to_enumeration_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    complete_current_transfer(bad_device_header, sizeof(bad_device_header));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ERROR, g_host.root_ports[0].state);
    TEST_ASSERT_EQUAL(xUSBH_ENUMERATION_ERROR, g_host.enumeration.state);
}

void test_enumeration_rejects_config_header_larger_than_buffer(void)
{
    uint8_t bad_config_header[USB_CONFIGURATION_DESC_LEN] = {
        USB_CONFIGURATION_DESC_LEN, USB_DESC_TYPE_CONFIGURATION, 0xFFU, 0x7FU, 1U, 1U, 0U, 0x80U, 50U,
    };

    init_and_start_host(&g_host);
    drive_to_enumeration_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    complete_current_transfer(valid_device_descriptor, 8U);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    complete_current_transfer(NULL, 0U);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    complete_current_transfer(valid_device_descriptor, USB_DEVICE_DESC_LEN);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    complete_current_transfer(bad_config_header, sizeof(bad_config_header));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ERROR, g_host.root_ports[0].state);
}

void test_enumeration_rejects_duplicate_active_endpoint_address(void)
{
    init_and_start_host(&g_host);
    drive_to_enumeration_start();
    drive_to_config_full_request(duplicate_endpoint_config_descriptor, sizeof(duplicate_endpoint_config_descriptor));
    complete_current_transfer(duplicate_endpoint_config_descriptor, sizeof(duplicate_endpoint_config_descriptor));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_UNSUPPORTED_DESCRIPTOR, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ERROR, g_host.root_ports[0].state);
    TEST_ASSERT_FALSE(g_host.interfaces[0].is_allocated);
    TEST_ASSERT_FALSE(g_host.endpoints[0].is_allocated);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_enumeration_success_runs_control_flow_to_configured);
    RUN_TEST(test_enumeration_times_out_pending_control_transfer);
    RUN_TEST(test_enumeration_rejects_invalid_ep0_mps_for_speed);
    RUN_TEST(test_enumeration_rejects_config_header_larger_than_buffer);
    RUN_TEST(test_enumeration_rejects_duplicate_active_endpoint_address);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
