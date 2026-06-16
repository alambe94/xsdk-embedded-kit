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

// @file test_standard_requests.c
// @brief Host tests for standard USB request handling via the full DCD
// event path (xUSBD_DCD_Event_Callback -> setup_request_process).

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

// CLASS PROBE DRIVER //////////////////////////////////////////////////////////

typedef struct
{
    uint8_t interface;
    uint32_t control_in_calls;
    uint32_t control_out_calls;
    uint32_t control_complete_calls;
} Class_Probe_t;

static Class_Probe_t *class_probe_from(xUSBD_Class_Context_t *class_ctx)
{
    void *ctx = NULL;
    return (xUSBD_Class_Get_App_Context(class_ctx, &ctx) == xRETURN_OK) ? ctx : NULL;
}

static xRETURN_t probe_init_instance(xUSBD_Class_Context_t *class_ctx)
{
    Class_Probe_t *probe = class_probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }
    return xUSBD_Class_Allocate_Interface(class_ctx, &probe->interface);
}

static uint32_t probe_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    (void)speed;
    return (uint32_t)(build_interface_descriptor(buffer, class_ctx->first_interface, 0U, 0U, USB_CLASS_VENDOR, 0U, 0U, 0U) - buffer);
}

static xRETURN_t probe_control_in(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    static uint8_t response_data = 0xA5U;

    Class_Probe_t *probe = class_probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    probe->control_in_calls++;
    response->data = &response_data;
    response->length = sizeof(response_data);
    return xRETURN_OK;
}

static xRETURN_t probe_control_out(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    (void)data;
    (void)length;

    Class_Probe_t *probe = class_probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    probe->control_out_calls++;
    return xRETURN_OK;
}

static xRETURN_t probe_control_complete(xUSBD_Class_Context_t *class_ctx, USB_Setup_Request_t *request)
{
    (void)request;

    Class_Probe_t *probe = class_probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    probe->control_complete_calls++;
    return xRETURN_OK;
}

static xUSBD_Class_Driver_t probe_driver = {
    .init_instance = probe_init_instance,
    .build_descriptor = probe_build_descriptor,
    .control_in_request = probe_control_in,
    .control_out_request = probe_control_out,
    .control_transfer_complete = probe_control_complete,
};

// FAILING OUT DRIVER //////////////////////////////////////////////////////////
// Used by test_out_data_stage_failure_stalls_ep0.

static xRETURN_t failing_out_init(xUSBD_Class_Context_t *class_ctx)
{
    uint8_t interface = 0U;
    return xUSBD_Class_Allocate_Interface(class_ctx, &interface);
}

static xRETURN_t failing_out_handler(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    (void)class_ctx;
    (void)data;
    (void)length;
    return xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST;
}

static xUSBD_Class_Driver_t failing_out_driver = {
    .init_instance = failing_out_init,
    .build_descriptor = normal_build_descriptor,
    .control_out_request = failing_out_handler,
};

static uint8_t mos_test_data[] = {'x', 0, 'U', 0, 'S', 0, 'B', 0, 0, 0};

static xUSBD_MOS_Property_t mos_test_props[] = {
    xUSBD_MOS_Property("DeviceLabel", mos_test_data, sizeof(mos_test_data)),
    {0},
};

// SETUP HELPERS ///////////////////////////////////////////////////////////////

static void prepare_connected_device_with_probe(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx, Class_Probe_t *probe)
{
    memset(class_ctx, 0, sizeof(*class_ctx));
    memset(probe, 0, sizeof(*probe));
    (void)xUSBD_Class_Set_App_Context(class_ctx, probe);
    test_device_init(device_ctx);
    (void)test_class_register(device_ctx, class_ctx, &probe_driver, NULL);
    (void)test_device_start(device_ctx);
    dcd_fire_event(device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);
}

static void prepare_started_device_with_mos(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx)
{
    prepare_startable_device(device_ctx, class_ctx);
    (void)xUSBD_Class_Set_MOS_Properties(class_ctx, mos_test_props);
    (void)test_device_start(device_ctx);
    dcd_fire_event(device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);
}

static USB_Setup_Request_t get_descriptor_request(uint8_t descriptor_type, uint8_t descriptor_index, uint16_t length)
{
    USB_Setup_Request_t request = {0};
    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_DESCRIPTOR;
    request.wValue = xCPU_TO_LE16((uint16_t)(((uint16_t)descriptor_type << 8U) | descriptor_index));
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(length);
    return request;
}

// TESTS: GET_DESCRIPTOR ///////////////////////////////////////////////////////

void test_get_descriptor_returns_device_descriptor(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Class_Probe_t probe;
    USB_Setup_Request_t request = {0};

    prepare_connected_device_with_probe(&device_ctx, &class_ctx, &probe);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_DESCRIPTOR;
    request.wValue = xCPU_TO_LE16((uint16_t)((uint16_t)USB_DESC_TYPE_DEVICE << 8U));
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(18U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_returns_config_descriptor(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Class_Probe_t probe;
    USB_Setup_Request_t request = {0};

    prepare_connected_device_with_probe(&device_ctx, &class_ctx, &probe);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_DESCRIPTOR;
    request.wValue = xCPU_TO_LE16((uint16_t)((uint16_t)USB_DESC_TYPE_CONFIGURATION << 8U));
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(256U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_returns_vendor_string(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Class_Probe_t probe;
    USB_Setup_Request_t request = {0};

    prepare_connected_device_with_probe(&device_ctx, &class_ctx, &probe);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_DESCRIPTOR;
    request.wValue = xCPU_TO_LE16((uint16_t)((uint16_t)USB_DESC_TYPE_STRING << 8U) | (uint16_t)xUSBD_VENDOR_STRING_INDEX);
    request.wIndex = xCPU_TO_LE16(0x0409U);
    request.wLength = xCPU_TO_LE16(256U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_returns_product_and_serial_strings(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;

    request = get_descriptor_request(USB_DESC_TYPE_STRING, xUSBD_PRODUCT_STRING_INDEX, 256U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(10U, fake_dcd_ep_send_fake.arg3_val);
    TEST_ASSERT_EQUAL_UINT8('x', fake_dcd_ep_send_fake.arg2_val[2]);
    TEST_ASSERT_EQUAL_UINT8('B', fake_dcd_ep_send_fake.arg2_val[8]);

    request = get_descriptor_request(USB_DESC_TYPE_STRING, xUSBD_SERIAL_STRING_INDEX, 256U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 2U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(10U, fake_dcd_ep_send_fake.arg3_val);
    TEST_ASSERT_EQUAL_UINT8('0', fake_dcd_ep_send_fake.arg2_val[2]);
    TEST_ASSERT_EQUAL_UINT8('1', fake_dcd_ep_send_fake.arg2_val[8]);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_returns_qualifier(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_DESCRIPTOR;
    request.wValue = xCPU_TO_LE16((uint16_t)((uint16_t)USB_DESC_TYPE_QUALIFIER << 8U));
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(10U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_returns_other_speed_descriptor(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    request = get_descriptor_request(USB_DESC_TYPE_OTHER_SPEED, 0U, 256U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_OTHER_SPEED, fake_dcd_ep_send_fake.arg2_val[1]);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_other_speed_does_not_corrupt_config(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    // 1. Request OTHER_SPEED_CONFIG descriptor
    request = get_descriptor_request(USB_DESC_TYPE_OTHER_SPEED, 0U, 256U);
    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_OTHER_SPEED, fake_dcd_ep_send_fake.arg2_val[1]);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);

    // 2. Request CONFIGURATION descriptor
    request = get_descriptor_request(USB_DESC_TYPE_CONFIGURATION, 0U, 256U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 2U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(USB_DESC_TYPE_CONFIGURATION, fake_dcd_ep_send_fake.arg2_val[1]); // Ensure it was rebuilt as config descriptor
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_returns_language_id_string(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_DESCRIPTOR;
    request.wValue = xCPU_TO_LE16((uint16_t)((uint16_t)USB_DESC_TYPE_STRING << 8U));
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(4U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_returns_class_string(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_startable_device(&device_ctx, &class_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_Interface_String(&class_ctx, "Iface"));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));

    request = get_descriptor_request(USB_DESC_TYPE_STRING, class_ctx.interface_string_index, 256U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(12U, fake_dcd_ep_send_fake.arg3_val);
    TEST_ASSERT_EQUAL_UINT8('I', fake_dcd_ep_send_fake.arg2_val[2]);
    TEST_ASSERT_EQUAL_UINT8('e', fake_dcd_ep_send_fake.arg2_val[10]);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_returns_msos_string_when_descriptor_exists(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device_with_mos(&device_ctx, &class_ctx);

    request = get_descriptor_request(USB_DESC_TYPE_STRING, xUSBD_MSOS_STRING_INDEX, 256U);
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(18U, fake_dcd_ep_send_fake.arg3_val);
    TEST_ASSERT_EQUAL_UINT8('M', fake_dcd_ep_send_fake.arg2_val[2]);
    TEST_ASSERT_EQUAL_UINT8(xUSBD_WINUSB_VENDOR_CODE, fake_dcd_ep_send_fake.arg2_val[16]);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_unknown_type_stalls_ep0(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_DESCRIPTOR;
    request.wValue = xCPU_TO_LE16((uint16_t)(0xFFU << 8U));
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_stall_before = fake_dcd_ep_stall_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_stall_before + 1U, fake_dcd_ep_stall_fake.call_count);
}

void test_get_descriptor_returns_bos_when_present(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    device_ctx.bos_length = 5U;
    device_ctx.bos_descriptor[0] = 5U;
    device_ctx.bos_descriptor[1] = USB_DESC_TYPE_BOS;
    device_ctx.bos_descriptor[2] = 5U;
    device_ctx.bos_descriptor[3] = 0U;
    device_ctx.bos_descriptor[4] = 0U;

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_DESCRIPTOR;
    request.wValue = xCPU_TO_LE16((uint16_t)((uint16_t)USB_DESC_TYPE_BOS << 8U));
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(256U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_vendor_msos_descriptor_request_returns_descriptor(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device_with_mos(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = xUSBD_WINUSB_VENDOR_CODE;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(0x0007U);
    request.wLength = xCPU_TO_LE16(xUSBD_MAX_MOS2_DESCRIPTOR_SIZE);

    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_PTR(device_ctx.mos2_descriptor, fake_dcd_ep_send_fake.arg2_val);
    TEST_ASSERT_EQUAL_UINT32(device_ctx.mos2_length, fake_dcd_ep_send_fake.arg3_val);
    TEST_ASSERT_EQUAL_UINT16(USB_MOS2_SET_HEADER_DESCRIPTOR,
                             (uint16_t)(fake_dcd_ep_send_fake.arg2_val[2] | ((uint16_t)fake_dcd_ep_send_fake.arg2_val[3] << 8U)));
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

void test_vendor_msos_descriptor_request_limits_length(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device_with_mos(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_VENDOR | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = xUSBD_WINUSB_VENDOR_CODE;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(0x0007U);
    request.wLength = xCPU_TO_LE16(10U); // Limit to 10 bytes (Header size)

    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_PTR(device_ctx.mos2_descriptor, fake_dcd_ep_send_fake.arg2_val);
    TEST_ASSERT_EQUAL_UINT32(10U, fake_dcd_ep_send_fake.arg3_val); // Should be limited to wLength
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

// TESTS: GET_STATUS ///////////////////////////////////////////////////////////

void test_get_status_returns_device_status(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    // Seed dirty buffer byte to catch memory leak
    device_ctx.control_data[1] = 0xFFU;

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_STATUS;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(2U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(0x00U, fake_dcd_ep_send_fake.arg2_val[1]); // Ensure second byte is initialized to 0
}

void test_get_status_routes_interface_recipient_to_class(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Class_Probe_t probe;
    USB_Setup_Request_t request = {0};

    prepare_connected_device_with_probe(&device_ctx, &class_ctx, &probe);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = USB_REQ_GET_STATUS;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(probe.interface);
    request.wLength = xCPU_TO_LE16(2U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, probe.control_in_calls);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
}

void test_get_status_returns_endpoint_stall_state(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    // Seed dirty buffer byte to catch memory leak
    device_ctx.control_data[1] = 0xFFU;

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_ENDPOINT;
    request.bRequest = USB_REQ_GET_STATUS;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(0x01U);
    request.wLength = xCPU_TO_LE16(2U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(0x00U, fake_dcd_ep_send_fake.arg2_val[1]); // Ensure second byte is initialized to 0
}

// TESTS: GET_CONFIGURATION ////////////////////////////////////////////////////

void test_get_configuration_returns_current_value(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);
    device_ctx.is_addressed = true;
    device_ctx.configuration_value = 0U;

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_GET_CONFIGURATION;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(1U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_stall_fake.call_count);
}

// TESTS: SET_FEATURE / CLEAR_FEATURE //////////////////////////////////////////

void test_set_feature_enables_remote_wakeup(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_SET_FEATURE;
    request.wValue = xCPU_TO_LE16(1U); // DEVICE_REMOTE_WAKEUP
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_set_remote_wakeup_fake.call_count);
    TEST_ASSERT_TRUE(device_ctx.is_remote_wake_enabled);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
}

void test_clear_feature_disables_remote_wakeup(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);
    device_ctx.is_remote_wake_enabled = true;

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_CLEAR_FEATURE;
    request.wValue = xCPU_TO_LE16(1U); // DEVICE_REMOTE_WAKEUP
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_set_remote_wakeup_fake.call_count);
    TEST_ASSERT_FALSE(device_ctx.is_remote_wake_enabled);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
}

void test_clear_feature_clears_endpoint_stall(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_ENDPOINT;
    request.bRequest = USB_REQ_CLEAR_FEATURE;
    request.wValue = xCPU_TO_LE16(0U); // ENDPOINT_HALT
    request.wIndex = xCPU_TO_LE16(0x01U);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_clear_before = fake_dcd_ep_clear_stall_fake.call_count;
    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_clear_before + 1U, fake_dcd_ep_clear_stall_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
}

void test_clear_feature_invalid_recipient_stalls_ep0(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = USB_REQ_CLEAR_FEATURE;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_stall_before = fake_dcd_ep_stall_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_stall_before + 1U, fake_dcd_ep_stall_fake.call_count);
}

void test_set_feature_stalls_endpoint(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_ENDPOINT;
    request.bRequest = USB_REQ_SET_FEATURE;
    request.wValue = xCPU_TO_LE16(0U); // ENDPOINT_HALT
    request.wIndex = xCPU_TO_LE16(0x01U);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_stall_before = fake_dcd_ep_stall_fake.call_count;
    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_stall_before + 1U, fake_dcd_ep_stall_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
}

// TESTS: SET_CONFIGURATION / SET_INTERFACE / GET_INTERFACE ////////////////////

void test_set_configuration_not_addressed_stalls_ep0(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Class_Probe_t probe;
    USB_Setup_Request_t request = {0};

    prepare_connected_device_with_probe(&device_ctx, &class_ctx, &probe);

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = USB_REQ_SET_CONFIGURATION;
    request.wValue = xCPU_TO_LE16(1U);
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_stall_before = fake_dcd_ep_stall_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_stall_before + 1U, fake_dcd_ep_stall_fake.call_count);
}

void test_set_interface_routes_to_class(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Class_Probe_t probe;
    USB_Setup_Request_t request = {0};

    prepare_connected_device_with_probe(&device_ctx, &class_ctx, &probe);

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = USB_REQ_SET_INTERFACE;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(probe.interface);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, probe.control_out_calls);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
}

void test_get_interface_routes_to_class(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Class_Probe_t probe;
    USB_Setup_Request_t request = {0};

    prepare_connected_device_with_probe(&device_ctx, &class_ctx, &probe);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = USB_REQ_GET_INTERFACE;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(probe.interface);
    request.wLength = xCPU_TO_LE16(1U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(1U, probe.control_in_calls);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
}

// TESTS: unknown request + OUT data-stage error ///////////////////////////////

void test_unknown_standard_request_stalls_ep0(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    USB_Setup_Request_t request = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE;
    request.bRequest = 0xFFU;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(0U);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_stall_before = fake_dcd_ep_stall_fake.call_count;
    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    TEST_ASSERT_EQUAL_UINT32(ep_stall_before + 1U, fake_dcd_ep_stall_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before, fake_dcd_ep_send_fake.call_count);
}

void test_out_data_stage_failure_stalls_ep0(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    USB_Setup_Request_t request = {0};
    uint8_t out_data[2] = {0x01U, 0x02U};

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &failing_out_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = 0x09U;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(class_ctx.first_interface);
    request.wLength = xCPU_TO_LE16(2U);

    uint32_t ep_receive_before = fake_dcd_ep_receive_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));
    TEST_ASSERT_EQUAL_UINT32(ep_receive_before + 1U, fake_dcd_ep_receive_fake.call_count);

    uint32_t ep_stall_before = fake_dcd_ep_stall_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_DATA_RECEIVED, 0x00U, out_data, sizeof(out_data));
    TEST_ASSERT_EQUAL_UINT32(ep_stall_before + 1U, fake_dcd_ep_stall_fake.call_count);
}

// TESTS: class IN/OUT via full SETUP path /////////////////////////////////////

void test_class_in_request_routes_via_setup_event_path(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Class_Probe_t probe;
    USB_Setup_Request_t request = {0};

    prepare_connected_device_with_probe(&device_ctx, &class_ctx, &probe);

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = 0x01U;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(probe.interface);
    request.wLength = xCPU_TO_LE16(1U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));
    TEST_ASSERT_EQUAL_UINT32(1U, probe.control_in_calls);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);

    dcd_fire_event(&device_ctx, USB_DCD_DATA_SENT, 0x80U, NULL, 0U);
    dcd_fire_event(&device_ctx, USB_DCD_DATA_RECEIVED, 0x00U, NULL, 0U);
    TEST_ASSERT_EQUAL_UINT32(1U, probe.control_complete_calls);
}

void test_class_out_request_with_data_stage_completes(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Class_Probe_t probe;
    USB_Setup_Request_t request = {0};
    uint8_t out_data[2] = {0x01U, 0x02U};

    prepare_connected_device_with_probe(&device_ctx, &class_ctx, &probe);

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = 0x09U;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(probe.interface);
    request.wLength = xCPU_TO_LE16(2U);

    uint32_t ep_receive_before = fake_dcd_ep_receive_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));
    TEST_ASSERT_EQUAL_UINT32(ep_receive_before + 1U, fake_dcd_ep_receive_fake.call_count);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_DATA_RECEIVED, 0x00U, out_data, sizeof(out_data));
    TEST_ASSERT_EQUAL_UINT32(1U, probe.control_out_calls);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);

    dcd_fire_event(&device_ctx, USB_DCD_DATA_SENT, 0x80U, NULL, 0U);
    TEST_ASSERT_EQUAL_UINT32(1U, probe.control_complete_calls);
}

static xRETURN_t no_ctrl_init(xUSBD_Class_Context_t *class_ctx)
{
    uint8_t interface = 0U;
    return xUSBD_Class_Allocate_Interface(class_ctx, &interface);
}

static xUSBD_Class_Driver_t no_ctrl_driver = {
    .init_instance = no_ctrl_init,
    .build_descriptor = normal_build_descriptor,
};

void test_set_interface_zero_defaults_to_ok_when_unhandled(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    USB_Setup_Request_t request = {0};

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &no_ctrl_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    // Request SET_INTERFACE to alternate setting 0
    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = USB_REQ_SET_INTERFACE;
    request.wValue = xCPU_TO_LE16(0U); // alternate setting 0
    request.wIndex = xCPU_TO_LE16(class_ctx.first_interface);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    uint32_t ep_stall_before = fake_dcd_ep_stall_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    // Should not stall, but reply with success (ZLP status phase)
    TEST_ASSERT_EQUAL_UINT32(ep_stall_before, fake_dcd_ep_stall_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
}

void test_get_interface_defaults_to_zero_when_unhandled(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    USB_Setup_Request_t request = {0};

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &no_ctrl_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    // Request GET_INTERFACE
    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = USB_REQ_GET_INTERFACE;
    request.wValue = xCPU_TO_LE16(0U);
    request.wIndex = xCPU_TO_LE16(class_ctx.first_interface);
    request.wLength = xCPU_TO_LE16(1U);

    uint32_t ep_send_before = fake_dcd_ep_send_fake.call_count;
    uint32_t ep_stall_before = fake_dcd_ep_stall_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    // Should not stall, and return alternate setting 0 (1-byte value 0x00)
    TEST_ASSERT_EQUAL_UINT32(ep_stall_before, fake_dcd_ep_stall_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(ep_send_before + 1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.arg3_val);
    TEST_ASSERT_EQUAL_UINT8(0x00U, fake_dcd_ep_send_fake.arg2_val[0]);
}

void test_set_interface_non_zero_unhandled_stalls_ep0(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    USB_Setup_Request_t request = {0};

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &no_ctrl_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_device_start(&device_ctx));
    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    // Request SET_INTERFACE to alternate setting 1 (unsupported)
    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_INTERFACE;
    request.bRequest = USB_REQ_SET_INTERFACE;
    request.wValue = xCPU_TO_LE16(1U); // alternate setting 1
    request.wIndex = xCPU_TO_LE16(class_ctx.first_interface);
    request.wLength = xCPU_TO_LE16(0U);

    uint32_t ep_stall_before = fake_dcd_ep_stall_fake.call_count;
    dcd_fire_event(&device_ctx, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)&request, sizeof(request));

    // Should stall
    TEST_ASSERT_EQUAL_UINT32(ep_stall_before + 1U, fake_dcd_ep_stall_fake.call_count);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_get_descriptor_returns_device_descriptor);
    RUN_TEST(test_get_descriptor_returns_config_descriptor);
    RUN_TEST(test_get_descriptor_returns_vendor_string);
    RUN_TEST(test_get_descriptor_returns_product_and_serial_strings);
    RUN_TEST(test_get_descriptor_returns_qualifier);
    RUN_TEST(test_get_descriptor_returns_other_speed_descriptor);
    RUN_TEST(test_get_descriptor_other_speed_does_not_corrupt_config);
    RUN_TEST(test_get_descriptor_returns_language_id_string);
    RUN_TEST(test_get_descriptor_returns_class_string);
    RUN_TEST(test_get_descriptor_returns_msos_string_when_descriptor_exists);
    RUN_TEST(test_get_descriptor_unknown_type_stalls_ep0);
    RUN_TEST(test_get_descriptor_returns_bos_when_present);
    RUN_TEST(test_vendor_msos_descriptor_request_returns_descriptor);
    RUN_TEST(test_vendor_msos_descriptor_request_limits_length);
    RUN_TEST(test_get_status_returns_device_status);
    RUN_TEST(test_get_status_routes_interface_recipient_to_class);
    RUN_TEST(test_get_status_returns_endpoint_stall_state);
    RUN_TEST(test_get_configuration_returns_current_value);
    RUN_TEST(test_set_feature_enables_remote_wakeup);
    RUN_TEST(test_clear_feature_disables_remote_wakeup);
    RUN_TEST(test_clear_feature_clears_endpoint_stall);
    RUN_TEST(test_clear_feature_invalid_recipient_stalls_ep0);
    RUN_TEST(test_set_feature_stalls_endpoint);
    RUN_TEST(test_set_configuration_not_addressed_stalls_ep0);
    RUN_TEST(test_set_interface_routes_to_class);
    RUN_TEST(test_get_interface_routes_to_class);
    RUN_TEST(test_unknown_standard_request_stalls_ep0);
    RUN_TEST(test_out_data_stage_failure_stalls_ep0);
    RUN_TEST(test_class_in_request_routes_via_setup_event_path);
    RUN_TEST(test_class_out_request_with_data_stage_completes);
    RUN_TEST(test_set_interface_zero_defaults_to_ok_when_unhandled);
    RUN_TEST(test_get_interface_defaults_to_zero_when_unhandled);
    RUN_TEST(test_set_interface_non_zero_unhandled_stalls_ep0);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////
