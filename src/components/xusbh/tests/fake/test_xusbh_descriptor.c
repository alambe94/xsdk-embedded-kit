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

// @file test_xusbh_descriptor.c
// @brief Host tests for descriptor parsing and setup-packet builders.

#include "unity.h"

#include "xusb_setup.h"
#include "xusbh_descriptor.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_device_descriptor_parse_decodes_little_endian_fields(void)
{
    const uint8_t raw_device_descriptor[USB_DEVICE_DESC_LEN] = {
        USB_DEVICE_DESC_LEN,
        USB_DESC_TYPE_DEVICE,
        0x00U,
        0x02U,
        0xEFU,
        0x02U,
        0x01U,
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
    xUSBH_Device_Descriptor_t descriptor = {0};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Descriptor_Parse(raw_device_descriptor, sizeof(raw_device_descriptor), &descriptor));
    TEST_ASSERT_EQUAL_UINT16(0x0200U, descriptor.bcd_usb);
    TEST_ASSERT_EQUAL_UINT8(USB_CLASS_IAD_DEVICE, descriptor.device_class);
    TEST_ASSERT_EQUAL_UINT8(USB_IAD_DEVICE_SUBCLASS, descriptor.device_subclass);
    TEST_ASSERT_EQUAL_UINT8(USB_IAD_DEVICE_PROTOCOL, descriptor.device_protocol);
    TEST_ASSERT_EQUAL_UINT8(64U, descriptor.ep0_max_packet_size);
    TEST_ASSERT_EQUAL_UINT16(0x1234U, descriptor.vendor_id);
    TEST_ASSERT_EQUAL_UINT16(0x5678U, descriptor.product_id);
    TEST_ASSERT_EQUAL_UINT16(0x0100U, descriptor.bcd_device);
    TEST_ASSERT_EQUAL_UINT8(1U, descriptor.manufacturer_string_index);
    TEST_ASSERT_EQUAL_UINT8(2U, descriptor.product_string_index);
    TEST_ASSERT_EQUAL_UINT8(3U, descriptor.serial_string_index);
    TEST_ASSERT_EQUAL_UINT8(1U, descriptor.configuration_count);
}

void test_fixed_descriptor_parsers_reject_null_short_and_wrong_type_buffers(void)
{
    const uint8_t short_device_descriptor[] = {
        4U,
        USB_DESC_TYPE_DEVICE,
        0x00U,
        0x02U,
    };
    const uint8_t wrong_type_descriptor[USB_DEVICE_DESC_LEN] = {
        USB_DEVICE_DESC_LEN,
        USB_DESC_TYPE_CONFIGURATION,
    };
    xUSBH_Device_Descriptor_t descriptor = {0};

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Device_Descriptor_Parse(NULL, USB_DEVICE_DESC_LEN, &descriptor));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Device_Descriptor_Parse(wrong_type_descriptor, USB_DEVICE_DESC_LEN, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT,
                      xUSBH_Device_Descriptor_Parse(short_device_descriptor, sizeof(short_device_descriptor), &descriptor));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT,
                      xUSBH_Device_Descriptor_Parse(wrong_type_descriptor, sizeof(wrong_type_descriptor), &descriptor));
}

void test_configuration_interface_and_endpoint_parse_decode_fields(void)
{
    const uint8_t raw_config_descriptor[] = {
        USB_CONFIGURATION_DESC_LEN,
        USB_DESC_TYPE_CONFIGURATION,
        31U,
        0U,
        1U,
        1U,
        0U,
        0x80U,
        50U,
        USB_INTERFACE_DESC_LEN,
        USB_DESC_TYPE_INTERFACE,
        2U,
        1U,
        1U,
        USB_CLASS_HID,
        1U,
        1U,
        4U,
        6U,
        USB_DESC_TYPE_HID,
        0x11U,
        1U,
        0U,
        1U,
        USB_ENDPOINT_DESC_LEN,
        USB_DESC_TYPE_ENDPOINT,
        0x81U,
        USB_ENDP_TYPE_INTR,
        64U,
        0U,
        8U,
    };
    xUSBH_Configuration_Descriptor_t config = {0};
    xUSBH_Interface_Descriptor_t interface = {0};
    xUSBH_Endpoint_Descriptor_t endpoint = {0};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Configuration_Descriptor_Parse(raw_config_descriptor, sizeof(raw_config_descriptor), &config));
    TEST_ASSERT_EQUAL_UINT16(sizeof(raw_config_descriptor), config.total_length);
    TEST_ASSERT_EQUAL_UINT8(1U, config.interface_count);
    TEST_ASSERT_EQUAL_UINT8(1U, config.configuration_value);
    TEST_ASSERT_EQUAL_UINT8(0x80U, config.attributes);
    TEST_ASSERT_EQUAL_UINT8(50U, config.max_power);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Interface_Descriptor_Parse(&raw_config_descriptor[USB_CONFIGURATION_DESC_LEN],
                                                                   sizeof(raw_config_descriptor) - USB_CONFIGURATION_DESC_LEN, &interface));
    TEST_ASSERT_EQUAL_UINT8(2U, interface.interface_number);
    TEST_ASSERT_EQUAL_UINT8(1U, interface.alternate_setting);
    TEST_ASSERT_EQUAL_UINT8(1U, interface.endpoint_count);
    TEST_ASSERT_EQUAL_UINT8(USB_CLASS_HID, interface.class_code);
    TEST_ASSERT_EQUAL_UINT8(1U, interface.subclass);
    TEST_ASSERT_EQUAL_UINT8(1U, interface.protocol);
    TEST_ASSERT_EQUAL_UINT8(4U, interface.interface_string_index);

    TEST_ASSERT_EQUAL(xRETURN_OK,
                      xUSBH_Endpoint_Descriptor_Parse(&raw_config_descriptor[sizeof(raw_config_descriptor) - USB_ENDPOINT_DESC_LEN],
                                                      USB_ENDPOINT_DESC_LEN, &endpoint));
    TEST_ASSERT_EQUAL_UINT8(0x81U, endpoint.endpoint_address);
    TEST_ASSERT_EQUAL_UINT8(USB_ENDP_TYPE_INTR, endpoint.attributes);
    TEST_ASSERT_EQUAL_UINT8(USB_ENDP_TYPE_INTR, endpoint.endpoint_type);
    TEST_ASSERT_EQUAL_UINT16(64U, endpoint.max_packet_size);
    TEST_ASSERT_EQUAL_UINT8(8U, endpoint.interval);
}

void test_descriptor_walker_handles_class_specific_descriptors_without_recursion(void)
{
    const uint8_t raw_config_descriptor[] = {
        USB_CONFIGURATION_DESC_LEN,
        USB_DESC_TYPE_CONFIGURATION,
        31U,
        0U,
        1U,
        1U,
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
        6U,
        USB_DESC_TYPE_HID,
        0x11U,
        1U,
        0U,
        1U,
        USB_ENDPOINT_DESC_LEN,
        USB_DESC_TYPE_ENDPOINT,
        0x81U,
        USB_ENDP_TYPE_INTR,
        64U,
        0U,
        8U,
    };
    xUSBH_Descriptor_Walker_t walker = {0};
    xUSBH_Descriptor_Header_t descriptor = {0};
    bool has_descriptor = false;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Descriptor_Walker_Init(&walker, raw_config_descriptor, sizeof(raw_config_descriptor)));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Descriptor_Walker_Next(&walker, &descriptor, &has_descriptor));
    TEST_ASSERT_TRUE(has_descriptor);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_CONFIGURATION, descriptor.type);
    TEST_ASSERT_EQUAL_UINT32(0U, descriptor.offset);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Descriptor_Walker_Next(&walker, &descriptor, &has_descriptor));
    TEST_ASSERT_TRUE(has_descriptor);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_INTERFACE, descriptor.type);
    TEST_ASSERT_EQUAL_UINT32(USB_CONFIGURATION_DESC_LEN, descriptor.offset);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Descriptor_Walker_Next(&walker, &descriptor, &has_descriptor));
    TEST_ASSERT_TRUE(has_descriptor);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_HID, descriptor.type);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Descriptor_Walker_Next(&walker, &descriptor, &has_descriptor));
    TEST_ASSERT_TRUE(has_descriptor);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_ENDPOINT, descriptor.type);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Descriptor_Walker_Next(&walker, &descriptor, &has_descriptor));
    TEST_ASSERT_FALSE(has_descriptor);
}

void test_configuration_validate_rejects_malformed_descriptor_trees(void)
{
    const uint8_t total_length_too_long[] = {
        USB_CONFIGURATION_DESC_LEN, USB_DESC_TYPE_CONFIGURATION, 64U, 0U, 1U, 1U, 0U, 0x80U, 50U,
    };
    const uint8_t zero_length_child[] = {
        USB_CONFIGURATION_DESC_LEN, USB_DESC_TYPE_CONFIGURATION, 11U, 0U, 1U, 1U, 0U, 0x80U, 50U, 0U, USB_DESC_TYPE_INTERFACE,
    };
    const uint8_t overrun_child[] = {
        USB_CONFIGURATION_DESC_LEN, USB_DESC_TYPE_CONFIGURATION, 12U, 0U, 1U, 1U, 0U, 0x80U, 50U, 4U, USB_DESC_TYPE_INTERFACE, 0U,
    };
    uint16_t total_length = 0U;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT,
                      xUSBH_Configuration_Descriptor_Validate(total_length_too_long, sizeof(total_length_too_long), &total_length));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT,
                      xUSBH_Configuration_Descriptor_Validate(zero_length_child, sizeof(zero_length_child), &total_length));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT,
                      xUSBH_Configuration_Descriptor_Validate(overrun_child, sizeof(overrun_child), &total_length));
}

void test_configuration_validate_reports_total_length_for_complete_tree(void)
{
    const uint8_t raw_config_descriptor[] = {
        USB_CONFIGURATION_DESC_LEN,
        USB_DESC_TYPE_CONFIGURATION,
        25U,
        0U,
        1U,
        1U,
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
    uint16_t total_length = 0U;

    TEST_ASSERT_EQUAL(xRETURN_OK,
                      xUSBH_Configuration_Descriptor_Validate(raw_config_descriptor, sizeof(raw_config_descriptor), &total_length));
    TEST_ASSERT_EQUAL_UINT16(sizeof(raw_config_descriptor), total_length);
}

void test_setup_builders_create_standard_enumeration_requests(void)
{
    USB_Setup_Request_t request = {0};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Setup_Build_Get_Descriptor(&request, USB_DESC_TYPE_CONFIGURATION, 1U, 0U, 9U));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE, request.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_GET_DESCRIPTOR, request.bRequest);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_CONFIGURATION, xUSB_Setup_Get_Descriptor_Type(&request));
    TEST_ASSERT_EQUAL_UINT8(1U, xUSB_Setup_Get_Descriptor_Index(&request));
    TEST_ASSERT_EQUAL_UINT16(0U, xUSB_Setup_Get_Index(&request));
    TEST_ASSERT_EQUAL_UINT16(9U, xUSB_Setup_Get_Length(&request));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Setup_Build_Set_Address(&request, 7U));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE, request.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_SET_ADDRESS, request.bRequest);
    TEST_ASSERT_EQUAL_UINT16(7U, xUSB_Setup_Get_Value(&request));
    TEST_ASSERT_EQUAL_UINT16(0U, xUSB_Setup_Get_Length(&request));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Setup_Build_Get_Configuration(&request));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE, request.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_GET_CONFIGURATION, request.bRequest);
    TEST_ASSERT_EQUAL_UINT16(1U, xUSB_Setup_Get_Length(&request));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Setup_Build_Set_Configuration(&request, 2U));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE, request.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_SET_CONFIGURATION, request.bRequest);
    TEST_ASSERT_EQUAL_UINT16(2U, xUSB_Setup_Get_Value(&request));
}

void test_setup_builders_create_status_feature_and_interface_requests(void)
{
    USB_Setup_Request_t request = {0};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Setup_Build_Get_Status(&request, USB_REQ_RECIPIENT_ENDPOINT, 0x81U));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_ENDPOINT, request.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_GET_STATUS, request.bRequest);
    TEST_ASSERT_EQUAL_UINT16(0U, xUSB_Setup_Get_Value(&request));
    TEST_ASSERT_EQUAL_UINT16(0x81U, xUSB_Setup_Get_Index(&request));
    TEST_ASSERT_EQUAL_UINT16(2U, xUSB_Setup_Get_Length(&request));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Setup_Build_Clear_Feature(&request, USB_REQ_RECIPIENT_ENDPOINT, 0U, 0x81U));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_ENDPOINT, request.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_CLEAR_FEATURE, request.bRequest);
    TEST_ASSERT_EQUAL_UINT16(0U, xUSB_Setup_Get_Value(&request));
    TEST_ASSERT_EQUAL_UINT16(0x81U, xUSB_Setup_Get_Index(&request));
    TEST_ASSERT_EQUAL_UINT16(0U, xUSB_Setup_Get_Length(&request));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Setup_Build_Set_Feature(&request, USB_REQ_RECIPIENT_DEVICE, 1U, 0U));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE, request.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_SET_FEATURE, request.bRequest);
    TEST_ASSERT_EQUAL_UINT16(1U, xUSB_Setup_Get_Value(&request));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Setup_Build_Set_Interface(&request, 3U, 1U));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE, request.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_SET_INTERFACE, request.bRequest);
    TEST_ASSERT_EQUAL_UINT16(1U, xUSB_Setup_Get_Value(&request));
    TEST_ASSERT_EQUAL_UINT16(3U, xUSB_Setup_Get_Index(&request));
}

void test_setup_builders_reject_invalid_arguments(void)
{
    USB_Setup_Request_t request = {0};

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER,
                      xUSBH_Setup_Build_Get_Descriptor(NULL, USB_DESC_TYPE_DEVICE, 0U, 0U, USB_DEVICE_DESC_LEN));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Setup_Build_Set_Address(NULL, 1U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT, xUSBH_Setup_Build_Set_Address(&request, 128U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT, xUSBH_Setup_Build_Get_Status(&request, USB_REQ_RECIPIENT_MASK, 0U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT, xUSBH_Setup_Build_Clear_Feature(&request, USB_REQ_RECIPIENT_MASK, 0U, 0U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT, xUSBH_Setup_Build_Set_Feature(&request, USB_REQ_RECIPIENT_MASK, 0U, 0U));
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_device_descriptor_parse_decodes_little_endian_fields);
    RUN_TEST(test_fixed_descriptor_parsers_reject_null_short_and_wrong_type_buffers);
    RUN_TEST(test_configuration_interface_and_endpoint_parse_decode_fields);
    RUN_TEST(test_descriptor_walker_handles_class_specific_descriptors_without_recursion);
    RUN_TEST(test_configuration_validate_rejects_malformed_descriptor_trees);
    RUN_TEST(test_configuration_validate_reports_total_length_for_complete_tree);
    RUN_TEST(test_setup_builders_create_standard_enumeration_requests);
    RUN_TEST(test_setup_builders_create_status_feature_and_interface_requests);
    RUN_TEST(test_setup_builders_reject_invalid_arguments);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
