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

// @file test_descriptor.c
// @brief Host tests for xUSBD_Build_Config_Descriptor: successful build,
// overflow guard, and multi-class total-length correctness.

#include "unity.h"
#include "test_helpers.h"
#include "xusb_win_defs.h"

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
}
void tearDown(void)
{
}

// OVERSIZED DRIVER ////////////////////////////////////////////////////////////

static uint32_t oversized_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    (void)class_ctx;
    (void)buffer;
    (void)speed;
    return xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE;
}

static xUSBD_Class_Driver_t oversized_driver = {
    .init_instance = one_interface_init,
    .build_descriptor = oversized_build_descriptor,
};

static uint8_t winusb_guid_utf16[] = {
    '{', 0, '0', 0, '1', 0, '2', 0, '3', 0, '4', 0, '5', 0, '6', 0, '7', 0, '-', 0, '8', 0, '9', 0, 'A', 0,
    'B', 0, '-', 0, 'C', 0, 'D', 0, 'E', 0, 'F', 0, '-', 0, '0', 0, '1', 0, '2', 0, '3', 0, '-', 0, '4', 0,
    '5', 0, '6', 0, '7', 0, '8', 0, '9', 0, 'A', 0, 'B', 0, 'C', 0, 'D', 0, 'E', 0, 'F', 0, '}', 0, 0,   0,
};

static xUSBD_MOS_Property_t winusb_mos_props[] = {
    xUSBD_MOS_Property("DeviceInterfaceGUID", winusb_guid_utf16, sizeof(winusb_guid_utf16)),
    {0},
};

// TESTS ///////////////////////////////////////////////////////////////////////

void test_valid_descriptor_build_succeeds(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    uint8_t descriptor[xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE];

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Build_Config_Descriptor(&device_ctx, descriptor, xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
                                                                USB_DESC_TYPE_CONFIGURATION, USB_SPEED_HIGH));

    USB_Configuration_Descriptor_t *config = (USB_Configuration_Descriptor_t *)descriptor;
    TEST_ASSERT_EQUAL_UINT8(USB_CONFIGURATION_DESC_LEN, config->bLength);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_CONFIGURATION, config->bDescriptorType);
    TEST_ASSERT_EQUAL_UINT16(USB_CONFIGURATION_DESC_LEN + USB_INTERFACE_DESC_LEN, xLE16_TO_CPU(config->wTotalLength));
    TEST_ASSERT_EQUAL_UINT8(1U, config->bNumInterfaces);
}

void test_descriptor_overflow_does_not_touch_output_buffer(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    uint8_t descriptor[xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE];

    memset(descriptor, 0xA5, sizeof(descriptor));
    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &oversized_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE,
                      xUSBD_Build_Config_Descriptor(&device_ctx, descriptor, xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE, USB_DESC_TYPE_CONFIGURATION,
                                                    USB_SPEED_HIGH));

    for (uint32_t i = 0U; i < sizeof(descriptor); i++)
    {
        TEST_ASSERT_EQUAL_UINT8(0xA5U, descriptor[i]);
    }
}

void test_two_class_descriptor_has_correct_totals(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_a = {0};
    xUSBD_Class_Context_t class_b = {0};
    uint8_t descriptor[xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE];

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_a, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_b, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Build_Config_Descriptor(&device_ctx, descriptor, xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
                                                                USB_DESC_TYPE_CONFIGURATION, USB_SPEED_HIGH));

    USB_Configuration_Descriptor_t *config = (USB_Configuration_Descriptor_t *)descriptor;
    TEST_ASSERT_EQUAL_UINT8(2U, config->bNumInterfaces);
    TEST_ASSERT_EQUAL_UINT16(USB_CONFIGURATION_DESC_LEN + (2U * USB_INTERFACE_DESC_LEN), xLE16_TO_CPU(config->wTotalLength));
}

void test_build_config_descriptor_other_speed_type(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    uint8_t descriptor[xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE];

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Build_Config_Descriptor(&device_ctx, descriptor, xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
                                                                USB_DESC_TYPE_OTHER_SPEED, USB_SPEED_FULL));

    USB_Configuration_Descriptor_t *config = (USB_Configuration_Descriptor_t *)descriptor;
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_OTHER_SPEED, config->bDescriptorType);
    TEST_ASSERT_EQUAL_UINT8(1U, config->bNumInterfaces);
}

void test_build_iad_descriptor_produces_correct_layout(void)
{
    uint8_t buffer[USB_IAD_DESC_LEN + 2U];

    memset(buffer, 0, sizeof(buffer));

    uint8_t *end = build_iad_descriptor(buffer, 1U, 2U, 0xEFU, 0x02U, 0x01U, 0x04U);

    USB_Interface_Association_Descriptor_t *iad = (USB_Interface_Association_Descriptor_t *)buffer;
    TEST_ASSERT_EQUAL_PTR(buffer + USB_IAD_DESC_LEN, end);
    TEST_ASSERT_EQUAL_UINT8(USB_IAD_DESC_LEN, iad->bLength);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_IAD, iad->bDescriptorType);
    TEST_ASSERT_EQUAL_UINT8(1U, iad->bFirstInterface);
    TEST_ASSERT_EQUAL_UINT8(2U, iad->bInterfaceCount);
    TEST_ASSERT_EQUAL_UINT8(0xEFU, iad->bFunctionClass);
    TEST_ASSERT_EQUAL_UINT8(0x02U, iad->bFunctionSubClass);
    TEST_ASSERT_EQUAL_UINT8(0x01U, iad->bFunctionProtocol);
    TEST_ASSERT_EQUAL_UINT8(0x04U, iad->iFunction);
    TEST_ASSERT_EQUAL_UINT8(0U, buffer[USB_IAD_DESC_LEN]);
}

void test_connect_builds_super_speed_bos_and_msos_descriptor(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_MOS_Properties(&class_ctx, winusb_mos_props));

    fake_dcd_get_speed_fake.return_val = USB_SPEED_SUPER;
    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    USB_BOS_Descriptor_t *bos = (USB_BOS_Descriptor_t *)device_ctx.bos_descriptor;
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_BOS, bos->bDescriptorType);
    TEST_ASSERT_EQUAL_UINT8(3U, bos->bNumDeviceCaps);
    TEST_ASSERT_EQUAL_UINT16((uint16_t)(5U + 7U + 10U + 28U), xLE16_TO_CPU(bos->wTotalLength));
    TEST_ASSERT_EQUAL_UINT16(xLE16_TO_CPU(bos->wTotalLength), device_ctx.bos_length);

    USB_Device_Capability_Descriptor_t *usb2 = (USB_Device_Capability_Descriptor_t *)&device_ctx.bos_descriptor[5];
    TEST_ASSERT_EQUAL_UINT8(USB_CAPABILITY_20_EXTENTION, usb2->bDevCapabilityType);

    USB_SS_Device_Capability_Descriptor_t *ss = (USB_SS_Device_Capability_Descriptor_t *)&device_ctx.bos_descriptor[12];
    TEST_ASSERT_EQUAL_UINT8(USB_CAPABILITY_SUPER_SPEED_USB, ss->bDevCapabilityType);
    TEST_ASSERT_EQUAL_UINT16(0x000EU, xLE16_TO_CPU(ss->wSpeedsSupported));

    USB_MS_OS_20_Platform_Cap_t *platform = (USB_MS_OS_20_Platform_Cap_t *)&device_ctx.bos_descriptor[22];
    TEST_ASSERT_EQUAL_UINT8(28U, platform->bLength);
    TEST_ASSERT_EQUAL_UINT8(0x05U, platform->bDevCapabilityType);
    TEST_ASSERT_EQUAL_UINT8(xUSBD_WINUSB_VENDOR_CODE, platform->bMS_VendorCode);
    TEST_ASSERT_EQUAL_UINT16(device_ctx.mos2_length, platform->wMSOSDescriptorSetTotalLength);

    USB_MS_OS_20_Set_Header_Descriptor_t *set_header = (USB_MS_OS_20_Set_Header_Descriptor_t *)device_ctx.mos2_descriptor;
    TEST_ASSERT_EQUAL_UINT16(10U, set_header->wLength);
    TEST_ASSERT_EQUAL_UINT16(USB_MOS2_SET_HEADER_DESCRIPTOR, set_header->wDescriptorType);
    TEST_ASSERT_EQUAL_UINT16(device_ctx.mos2_length, set_header->wTotalLength);

    USB_MS_OS_20_Subset_Header_Function_t *function_header = (USB_MS_OS_20_Subset_Header_Function_t *)&device_ctx.mos2_descriptor[18];
    TEST_ASSERT_EQUAL_UINT16(USB_MOS2_SUBSET_HEADER_FUNCTION, function_header->wDescriptorType);
    TEST_ASSERT_EQUAL_UINT8(class_ctx.first_interface, function_header->bFirstInterface);
    TEST_ASSERT_GREATER_THAN_UINT16(8U, function_header->wSubsetLength);

    TEST_ASSERT_EQUAL_UINT16(0x0004U, (uint16_t)(device_ctx.mos2_descriptor[28] | ((uint16_t)device_ctx.mos2_descriptor[29] << 8U)));
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_valid_descriptor_build_succeeds);
    RUN_TEST(test_descriptor_overflow_does_not_touch_output_buffer);
    RUN_TEST(test_two_class_descriptor_has_correct_totals);
    RUN_TEST(test_build_config_descriptor_other_speed_type);
    RUN_TEST(test_build_iad_descriptor_produces_correct_layout);
    RUN_TEST(test_connect_builds_super_speed_bos_and_msos_descriptor);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////
