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

// @file test_shared_helpers.c
// @brief Host tests for shared xUSB setup and descriptor helpers.

#include "unity.h"

#include "xusb_descriptor.h"
#include "xusb_setup.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_setup_read_decodes_little_endian_fields(void)
{
    const uint8_t raw[xUSB_SETUP_REQUEST_SIZE] = {
        USB_REQ_TYPE_IN | USB_REQ_RECIPIENT_DEVICE, USB_REQ_GET_DESCRIPTOR, 0x34U, 0x12U, 0x78U, 0x56U, 0xEFU, 0xCDU,
    };
    USB_Setup_Request_t request = {0};

    TEST_ASSERT_TRUE(xUSB_Setup_Read(&request, raw, sizeof(raw)));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_IN | USB_REQ_RECIPIENT_DEVICE, request.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_GET_DESCRIPTOR, request.bRequest);
    TEST_ASSERT_EQUAL_UINT16(0x1234U, xUSB_Setup_Get_Value(&request));
    TEST_ASSERT_EQUAL_UINT16(0x5678U, xUSB_Setup_Get_Index(&request));
    TEST_ASSERT_EQUAL_UINT16(0xCDEFU, xUSB_Setup_Get_Length(&request));
    TEST_ASSERT_EQUAL_UINT8(0x12U, xUSB_Setup_Get_Descriptor_Type(&request));
    TEST_ASSERT_EQUAL_UINT8(0x34U, xUSB_Setup_Get_Descriptor_Index(&request));
}

void test_setup_write_encodes_little_endian_fields(void)
{
    USB_Setup_Request_t request = {
        .bRequestType = USB_REQ_TYPE_OUT | USB_REQ_RECIPIENT_ENDPOINT,
        .bRequest = USB_REQ_CLEAR_FEATURE,
    };
    uint8_t raw[xUSB_SETUP_REQUEST_SIZE] = {0};

    xUSB_Setup_Set_Value(&request, 0x0102U);
    xUSB_Setup_Set_Index(&request, 0x0304U);
    xUSB_Setup_Set_Length(&request, 0x0506U);

    TEST_ASSERT_TRUE(xUSB_Setup_Write(raw, sizeof(raw), &request));
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_OUT | USB_REQ_RECIPIENT_ENDPOINT, raw[0]);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_CLEAR_FEATURE, raw[1]);
    TEST_ASSERT_EQUAL_UINT8(0x02U, raw[2]);
    TEST_ASSERT_EQUAL_UINT8(0x01U, raw[3]);
    TEST_ASSERT_EQUAL_UINT8(0x04U, raw[4]);
    TEST_ASSERT_EQUAL_UINT8(0x03U, raw[5]);
    TEST_ASSERT_EQUAL_UINT8(0x06U, raw[6]);
    TEST_ASSERT_EQUAL_UINT8(0x05U, raw[7]);
}

void test_setup_helpers_reject_short_or_null_buffers(void)
{
    USB_Setup_Request_t request = {0};
    uint8_t raw[xUSB_SETUP_REQUEST_SIZE] = {0};

    TEST_ASSERT_FALSE(xUSB_Setup_Read(NULL, raw, sizeof(raw)));
    TEST_ASSERT_FALSE(xUSB_Setup_Read(&request, NULL, sizeof(raw)));
    TEST_ASSERT_FALSE(xUSB_Setup_Read(&request, raw, xUSB_SETUP_REQUEST_SIZE - 1U));
    TEST_ASSERT_FALSE(xUSB_Setup_Write(NULL, sizeof(raw), &request));
    TEST_ASSERT_FALSE(xUSB_Setup_Write(raw, sizeof(raw), NULL));
    TEST_ASSERT_FALSE(xUSB_Setup_Write(raw, xUSB_SETUP_REQUEST_SIZE - 1U, &request));
}

void test_descriptor_header_read_and_complete_validation(void)
{
    const uint8_t complete_descriptor[] = {
        USB_CONFIGURATION_DESC_LEN, USB_DESC_TYPE_CONFIGURATION, 0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
    };
    const uint8_t truncated_descriptor[] = {
        USB_CONFIGURATION_DESC_LEN,
        USB_DESC_TYPE_CONFIGURATION,
        0x01U,
    };
    const uint8_t invalid_length_descriptor[] = {
        1U,
        USB_DESC_TYPE_CONFIGURATION,
    };
    uint8_t descriptor_length = 0U;
    uint8_t descriptor_type = 0U;

    TEST_ASSERT_TRUE(xUSB_Descriptor_Read_Header(complete_descriptor, sizeof(complete_descriptor), &descriptor_length, &descriptor_type));
    TEST_ASSERT_EQUAL_UINT8(USB_CONFIGURATION_DESC_LEN, descriptor_length);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_CONFIGURATION, descriptor_type);
    TEST_ASSERT_TRUE(xUSB_Descriptor_Is_Complete(complete_descriptor, sizeof(complete_descriptor)));
    TEST_ASSERT_FALSE(xUSB_Descriptor_Is_Complete(truncated_descriptor, sizeof(truncated_descriptor)));
    TEST_ASSERT_FALSE(xUSB_Descriptor_Is_Complete(invalid_length_descriptor, sizeof(invalid_length_descriptor)));
    TEST_ASSERT_FALSE(xUSB_Descriptor_Read_Header(complete_descriptor, 1U, &descriptor_length, &descriptor_type));
    TEST_ASSERT_FALSE(xUSB_Descriptor_Read_Header(NULL, sizeof(complete_descriptor), &descriptor_length, &descriptor_type));
}

void test_descriptor_le_field_helpers_are_bounds_checked(void)
{
    uint8_t descriptor[8] = {0};
    uint8_t byte_value = 0U;
    uint16_t value16 = 0U;
    uint32_t value32 = 0U;

    TEST_ASSERT_TRUE(xUSB_Descriptor_Write_U8(descriptor, sizeof(descriptor), 0U, USB_CONFIGURATION_DESC_LEN));
    TEST_ASSERT_TRUE(xUSB_Descriptor_Write_LE16(descriptor, sizeof(descriptor), 2U, 0x1234U));
    TEST_ASSERT_TRUE(xUSB_Descriptor_Write_LE32(descriptor, sizeof(descriptor), 4U, 0x89ABCDEFU));

    TEST_ASSERT_TRUE(xUSB_Descriptor_Read_U8(descriptor, sizeof(descriptor), 0U, &byte_value));
    TEST_ASSERT_TRUE(xUSB_Descriptor_Read_LE16(descriptor, sizeof(descriptor), 2U, &value16));
    TEST_ASSERT_TRUE(xUSB_Descriptor_Read_LE32(descriptor, sizeof(descriptor), 4U, &value32));
    TEST_ASSERT_EQUAL_UINT8(USB_CONFIGURATION_DESC_LEN, byte_value);
    TEST_ASSERT_EQUAL_UINT16(0x1234U, value16);
    TEST_ASSERT_EQUAL_UINT32(0x89ABCDEFU, value32);

    TEST_ASSERT_FALSE(xUSB_Descriptor_Read_U8(descriptor, sizeof(descriptor), sizeof(descriptor), &byte_value));
    TEST_ASSERT_FALSE(xUSB_Descriptor_Read_LE16(descriptor, sizeof(descriptor), 7U, &value16));
    TEST_ASSERT_FALSE(xUSB_Descriptor_Read_LE32(descriptor, sizeof(descriptor), 5U, &value32));
    TEST_ASSERT_FALSE(xUSB_Descriptor_Write_U8(descriptor, sizeof(descriptor), sizeof(descriptor), 0U));
    TEST_ASSERT_FALSE(xUSB_Descriptor_Write_LE16(descriptor, sizeof(descriptor), 7U, 0U));
    TEST_ASSERT_FALSE(xUSB_Descriptor_Write_LE32(descriptor, sizeof(descriptor), 5U, 0U));
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_setup_read_decodes_little_endian_fields);
    RUN_TEST(test_setup_write_encodes_little_endian_fields);
    RUN_TEST(test_setup_helpers_reject_short_or_null_buffers);
    RUN_TEST(test_descriptor_header_read_and_complete_validation);
    RUN_TEST(test_descriptor_le_field_helpers_are_bounds_checked);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
