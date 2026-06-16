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

// @file test_class_registration.c
// @brief Host tests for xUSBD_Class_Register, resource allocation,
// rollback on exhaustion, and app-context accessors.

#include "unity.h"
#include "test_helpers.h"

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
}
void tearDown(void)
{
}

// OVERFLOW DRIVERS ////////////////////////////////////////////////////////////

static xRETURN_t too_many_interfaces_init(xUSBD_Class_Context_t *class_ctx)
{
    xRETURN_t status = xRETURN_OK;
    for (uint32_t i = 0U; i <= xUSBD_MAX_INTERFACE_COUNT; i++)
    {
        uint8_t interface = 0U;
        status = xUSBD_Class_Allocate_Interface(class_ctx, &interface);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }
    return status;
}

static xRETURN_t too_many_in_endpoints_init(xUSBD_Class_Context_t *class_ctx)
{
    xRETURN_t status = xRETURN_OK;
    for (uint32_t i = 0U; i < xUSBD_MAX_ENDPOINT_MAP_ENTRIES; i++)
    {
        uint8_t ep_addr = 0U;
        status = xUSBD_Class_Allocate_Endpoint(class_ctx, 0x80U, &ep_addr);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }
    return status;
}

static xRETURN_t too_many_strings_init(xUSBD_Class_Context_t *class_ctx)
{
    xRETURN_t status = xRETURN_OK;
    for (uint32_t i = 0U; i <= xUSBD_MAX_STRING_MAP_ENTRIES; i++)
    {
        uint8_t string_index = 0U;
        status = xUSBD_Class_Allocate_String(class_ctx, &string_index);
        if (status != xRETURN_OK)
        {
            return status;
        }
    }
    return status;
}

static xUSBD_Class_Driver_t too_many_interfaces_driver = {
    .init_instance = too_many_interfaces_init,
    .build_descriptor = normal_build_descriptor,
};

static xUSBD_Class_Driver_t too_many_in_endpoints_driver = {
    .init_instance = too_many_in_endpoints_init,
    .build_descriptor = normal_build_descriptor,
};

static xUSBD_Class_Driver_t too_many_strings_driver = {
    .init_instance = too_many_strings_init,
    .build_descriptor = normal_build_descriptor,
};

// TESTS ///////////////////////////////////////////////////////////////////////

void test_class_register_validates_public_config(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Register(NULL, &class_ctx, &normal_driver));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Register(&device_ctx, NULL, &normal_driver));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Register(&device_ctx, &class_ctx, NULL));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Register(&device_ctx, &class_ctx, &normal_driver));
    TEST_ASSERT_EQUAL_PTR(&class_ctx, device_ctx.class_list_head);
}

void test_interface_exhaustion_rolls_back_registration(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED,
                      test_class_register(&device_ctx, &class_ctx, &too_many_interfaces_driver, NULL));
    TEST_ASSERT_NULL(device_ctx.class_list_head);
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.next_interface);
}

void test_endpoint_exhaustion_rolls_back_registration(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED,
                      test_class_register(&device_ctx, &class_ctx, &too_many_in_endpoints_driver, NULL));
    TEST_ASSERT_NULL(device_ctx.class_list_head);
    TEST_ASSERT_EQUAL_UINT8(0x81U, device_ctx.next_in_ep);
}

void test_string_exhaustion_rolls_back_registration(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_RESOURCE_EXHAUSTED, test_class_register(&device_ctx, &class_ctx, &too_many_strings_driver, NULL));
    TEST_ASSERT_NULL(device_ctx.class_list_head);
    TEST_ASSERT_EQUAL_UINT8(0x04U, device_ctx.next_string_index);
}

void test_allocation_requires_valid_arguments(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    uint8_t value = 0U;

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Allocate_Interface(NULL, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Allocate_Interface(&class_ctx, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Allocate_Endpoint(NULL, 0x80U, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Allocate_Endpoint(&class_ctx, 0x80U, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Allocate_String(NULL, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Allocate_String(&class_ctx, &value));

    class_ctx.device_ctx = &device_ctx;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Allocate_Interface(&class_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Allocate_Endpoint(&class_ctx, 0x80U, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Allocate_String(&class_ctx, NULL));
}

void test_allocation_requires_active_registration(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    uint8_t value = 0U;

    test_device_init(&device_ctx);

    class_ctx.device_ctx = &device_ctx;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_INVALID_CONFIGURATION, xUSBD_Class_Allocate_Interface(&class_ctx, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_INVALID_CONFIGURATION, xUSBD_Class_Allocate_Endpoint(&class_ctx, 0x80U, &value));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_INVALID_CONFIGURATION, xUSBD_Class_Allocate_String(&class_ctx, &value));
    TEST_ASSERT_EQUAL_UINT8(0U, device_ctx.next_interface);
    TEST_ASSERT_EQUAL_UINT8(0x81U, device_ctx.next_in_ep);
    TEST_ASSERT_EQUAL_UINT8(0x04U, device_ctx.next_string_index);
}

void test_class_app_context_accessors_validate_arguments(void)
{
    xUSBD_Class_Context_t class_ctx = {0};
    int dummy = 0;
    void *app_context = NULL;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Set_App_Context(NULL, &dummy));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_App_Context(NULL, &app_context));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_App_Context(&class_ctx, NULL));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_App_Context(&class_ctx, &dummy));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_App_Context(&class_ctx, &app_context));
    TEST_ASSERT_EQUAL_PTR(&dummy, app_context);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_App_Context(&class_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_App_Context(&class_ctx, &app_context));
    TEST_ASSERT_NULL(app_context);
}

// TESTS: class DCD EP helpers /////////////////////////////////////////////////

void test_class_dcd_ep_helpers_validate_null_arguments(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    uint8_t buf[8] = {0};

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_DCD_EP_Init(NULL, 0x81U, USB_ENDP_TYPE_BULK, 64U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_DCD_EP_Receive(NULL, 0x01U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_DCD_EP_Send(NULL, 0x81U, buf, sizeof(buf), false));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_DCD_EP_Stall(NULL, 0x81U));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_DCD_EP_Init(&class_ctx, 0x81U, USB_ENDP_TYPE_BULK, 64U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_DCD_EP_Receive(&class_ctx, 0x01U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_DCD_EP_Send(&class_ctx, 0x81U, buf, sizeof(buf), false));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_DCD_EP_Stall(&class_ctx, 0x81U));

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_NULL_POINTER, xUSBD_Class_DCD_EP_Init(&class_ctx, 0x81U, USB_ENDP_TYPE_BULK, 64U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_NULL_POINTER, xUSBD_Class_DCD_EP_Receive(&class_ctx, 0x01U, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_NULL_POINTER, xUSBD_Class_DCD_EP_Send(&class_ctx, 0x81U, buf, sizeof(buf), false));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_NULL_POINTER, xUSBD_Class_DCD_EP_Stall(&class_ctx, 0x81U));
}

void test_class_dcd_ep_helpers_forward_to_dcd(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    uint8_t data[8] = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_DCD_EP_Init(&class_ctx, 0x81U, USB_ENDP_TYPE_BULK, 64U));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_init_fake.call_count);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_DCD_EP_Receive(&class_ctx, 0x01U, data, sizeof(data)));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_receive_fake.call_count);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_DCD_EP_Send(&class_ctx, 0x81U, data, sizeof(data), false));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_DCD_EP_Stall(&class_ctx, 0x81U));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_stall_fake.call_count);
}

void test_dcd_transfer_queue_validates_inputs_and_optional_op(void)
{
    xUSBD_DCD_Transfer_t transfer = {
        .ep_addr = 0x81U,
    };
    xUSBD_DCD_Ops_t ops_without_queue = {
        .ep_send = fake_dcd_ep_send,
    };

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_NULL_POINTER, xUSBD_DCD_EP_Transfer_Queue(NULL, NULL, &transfer));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_DCD_EP_Transfer_Queue(&fake_dcd_ops, NULL, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_UNSUPPORTED_REQUEST, xUSBD_DCD_EP_Transfer_Queue(&ops_without_queue, NULL, &transfer));
}

void test_class_dcd_transfer_queue_forwards_to_optional_dcd_op(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    uint8_t data[8] = {0};
    xUSBD_DCD_Transfer_t transfer = {
        .ep_addr = 0x81U,
        .data = data,
        .length = sizeof(data),
        .is_zlp_required = true,
        .complete = NULL,
        .user_ctx = &class_ctx,
    };

    prepare_started_device(&device_ctx, &class_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_DCD_EP_Transfer_Queue(&class_ctx, &transfer));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_transfer_queue_fake.call_count);
    TEST_ASSERT_EQUAL_PTR(&transfer, fake_dcd_ep_transfer_queue_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT8(0x81U, fake_dcd_ep_transfer_queue_fake.arg1_val->ep_addr);
    TEST_ASSERT_EQUAL_UINT32(sizeof(data), fake_dcd_ep_transfer_queue_fake.arg1_val->length);
}

// TESTS: remaining class accessor APIs ////////////////////////////////////////

void test_class_speed_and_buffer_accessors_validate_arguments(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    USB_Speed_t speed = USB_SPEED_FULL;
    uint8_t *buffer = NULL;
    uint32_t length = 0U;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_Speed(NULL, &speed));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_Control_Buffer(NULL, &buffer, &length));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_Speed(&class_ctx, &speed));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_Control_Buffer(&class_ctx, &buffer, &length));

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_Speed(&class_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_Control_Buffer(&class_ctx, NULL, &length));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_Control_Buffer(&class_ctx, &buffer, NULL));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_Speed(&class_ctx, &speed));
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, speed);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_Control_Buffer(&class_ctx, &buffer, &length));
    TEST_ASSERT_EQUAL_PTR(device_ctx.control_data, buffer);
    TEST_ASSERT_EQUAL_UINT32(xUSBD_MAX_EP0_DATA_SIZE, length);
}

void test_class_ep_mps_accessors_roundtrip(void)
{
    xUSBD_Class_Context_t class_ctx = {0};
    uint16_t mps = 0U;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Set_EP_MPS(NULL, 512U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_EP_MPS(NULL, &mps));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_EP_MPS(&class_ctx, NULL));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_EP_MPS(&class_ctx, 512U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_EP_MPS(&class_ctx, &mps));
    TEST_ASSERT_EQUAL_UINT16(512U, mps);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_EP_MPS(&class_ctx, 64U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_EP_MPS(&class_ctx, &mps));
    TEST_ASSERT_EQUAL_UINT16(64U, mps);
}

void test_class_callbacks_accessors_roundtrip(void)
{
    xUSBD_Class_Context_t class_ctx = {0};
    int dummy = 0;
    void *callbacks = NULL;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Set_Callbacks(NULL, &dummy));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_Callbacks(NULL, &callbacks));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Get_Callbacks(&class_ctx, NULL));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_Callbacks(&class_ctx, &dummy));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_Callbacks(&class_ctx, &callbacks));
    TEST_ASSERT_EQUAL_PTR(&dummy, callbacks);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_Callbacks(&class_ctx, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_Callbacks(&class_ctx, &callbacks));
    TEST_ASSERT_NULL(callbacks);
}

void test_class_interface_string_and_mos_props_setters(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Set_Interface_String(NULL, "x"));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_NULL_POINTER, xUSBD_Class_Set_MOS_Properties(NULL, NULL));

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Register(&device_ctx, &class_ctx, &normal_driver));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_Interface_String(&class_ctx, "CDC Demo"));
    TEST_ASSERT_EQUAL_STRING("CDC Demo", class_ctx.interface_string);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_Interface_String(&class_ctx, NULL));
    TEST_ASSERT_NULL(class_ctx.interface_string);

    xUSBD_MOS_Property_t props[] = {{1U, "DeviceFriendlyName", NULL, 0}, {0}};
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_MOS_Properties(&class_ctx, props));
    TEST_ASSERT_EQUAL_PTR(props, class_ctx.mos_props);
}

// STRING DRIVER ///////////////////////////////////////////////////////////////

// TESTS: class get_string dispatch /////////////////////////////////////////////

void test_class_get_string_routes_to_owner(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx = {0};
    uint8_t *data = NULL;

    test_device_init(&device_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_ctx, &normal_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_Interface_String(&class_ctx, "Test"));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Get_String_Process(&device_ctx, class_ctx.interface_string_index, &data));
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_STRING("Test", (const char *)data);
}

void test_class_get_string_returns_not_supported_for_unknown_index(void)
{
    xUSBD_Device_Context_t device_ctx;
    uint8_t *data = NULL;

    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_REQ_GET_DESC_NOT_SUPPORTED, xUSBD_Class_Get_String_Process(&device_ctx, 0x04U, &data));
}

// TESTS: additional EP helper coverage /////////////////////////////////////////

void test_class_dcd_ep_deinit_forwards_to_dcd(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_started_device(&device_ctx, &class_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_DCD_EP_Deinit(&class_ctx, 0x01U));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_deinit_fake.call_count);
}

void test_class_dcd_ep_clear_stall_forwards_to_dcd(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;

    prepare_started_device(&device_ctx, &class_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_DCD_EP_Clear_Stall(&class_ctx, 0x01U));
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_clear_stall_fake.call_count);
}

// TESTS: lifecycle guard ///////////////////////////////////////////////////////

void test_register_rejected_after_start(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    xUSBD_Class_Context_t class_ctx2 = {0};

    prepare_started_device(&device_ctx, &class_ctx);

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_ALREADY_INITIALIZED, test_class_register(&device_ctx, &class_ctx2, &normal_driver, NULL));
    TEST_ASSERT_EQUAL_PTR(&class_ctx, device_ctx.class_list_head);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_class_register_validates_public_config);
    RUN_TEST(test_interface_exhaustion_rolls_back_registration);
    RUN_TEST(test_endpoint_exhaustion_rolls_back_registration);
    RUN_TEST(test_string_exhaustion_rolls_back_registration);
    RUN_TEST(test_allocation_requires_valid_arguments);
    RUN_TEST(test_allocation_requires_active_registration);
    RUN_TEST(test_class_app_context_accessors_validate_arguments);
    RUN_TEST(test_class_dcd_ep_helpers_validate_null_arguments);
    RUN_TEST(test_class_dcd_ep_helpers_forward_to_dcd);
    RUN_TEST(test_dcd_transfer_queue_validates_inputs_and_optional_op);
    RUN_TEST(test_class_dcd_transfer_queue_forwards_to_optional_dcd_op);
    RUN_TEST(test_class_speed_and_buffer_accessors_validate_arguments);
    RUN_TEST(test_class_ep_mps_accessors_roundtrip);
    RUN_TEST(test_class_callbacks_accessors_roundtrip);
    RUN_TEST(test_class_interface_string_and_mos_props_setters);
    RUN_TEST(test_class_get_string_routes_to_owner);
    RUN_TEST(test_class_get_string_returns_not_supported_for_unknown_index);
    RUN_TEST(test_class_dcd_ep_deinit_forwards_to_dcd);
    RUN_TEST(test_class_dcd_ep_clear_stall_forwards_to_dcd);
    RUN_TEST(test_register_rejected_after_start);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////
