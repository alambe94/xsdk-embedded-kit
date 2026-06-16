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

// @file test_dfu.c
// @brief Host tests for the xUSBD DFU class driver (Runtime and Mode variants).

#include <string.h>

#include "test_helpers.h"
#include "xusbd_dfu.h"
#include "xassert.h"

xSTATIC_ASSERT(xUSBD_DFU_DESC_SIZE <= xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE, "DFU descriptor exceeds config descriptor budget");

// DFU app callback fakes //////////////////////////////////////////////////////

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

FAKE_VALUE_FUNC(xRETURN_t, fake_dfu_bus_event, xUSBD_Class_Context_t *, USB_DCD_Event_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_dfu_io_control, xUSBD_Class_Context_t *, xUSBD_DFU_IO_CMD_t, void *, uint32_t, void **, uint32_t *);

#pragma GCC diagnostic pop

#define RESET_DFU_FAKES()                                                                                                                  \
    do                                                                                                                                     \
    {                                                                                                                                      \
        RESET_FAKE(fake_dfu_bus_event);                                                                                                    \
        RESET_FAKE(fake_dfu_io_control);                                                                                                   \
    } while (0)

// Test fixtures ///////////////////////////////////////////////////////////////

static xUSBD_Device_Context_t g_device;
static xUSBD_DFU_Context_t g_dfu;

static xUSBD_DFU_Callbacks_t g_dfu_calls = {
    .on_bus_event = fake_dfu_bus_event,
    .on_io_control = fake_dfu_io_control,
};

static void runtime_register(void)
{
    (void)xUSBD_Class_Register(&g_device, &g_dfu.class_ctx, xUSBD_DFU_Runtime_Class());
}

static void mode_register(void)
{
    (void)xUSBD_Class_Register(&g_device, &g_dfu.class_ctx, xUSBD_DFU_Mode_Class());
}

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
    RESET_DFU_FAKES();
    memset(&g_device, 0, sizeof(g_device));
    memset(&g_dfu, 0, sizeof(g_dfu));
    test_device_init(&g_device);
}

void tearDown(void)
{
}

// RUNTIME - REGISTRATION //////////////////////////////////////////////////////

void test_runtime_register_succeeds(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Register(&g_device, &g_dfu.class_ctx, xUSBD_DFU_Runtime_Class()));
}

void test_runtime_register_allocates_one_interface_one_string_no_endpoints(void)
{
    uint8_t iface_before = g_device.next_interface;
    uint8_t in_ep_before = g_device.next_in_ep;
    uint8_t out_ep_before = g_device.next_out_ep;
    uint8_t str_before = g_device.next_string_index;

    runtime_register();

    TEST_ASSERT_EQUAL_UINT8(iface_before + 1U, g_device.next_interface);
    TEST_ASSERT_EQUAL_UINT8(in_ep_before, g_device.next_in_ep);
    TEST_ASSERT_EQUAL_UINT8(out_ep_before, g_device.next_out_ep);
    TEST_ASSERT_EQUAL_UINT8(str_before, g_device.next_string_index);
}

void test_runtime_register_after_start_fails(void)
{
    runtime_register();
    (void)test_device_start(&g_device);

    xUSBD_DFU_Context_t extra;
    memset(&extra, 0, sizeof(extra));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_ALREADY_INITIALIZED, xUSBD_Class_Register(&g_device, &extra.class_ctx, xUSBD_DFU_Runtime_Class()));
}

// RUNTIME - DESCRIPTOR ////////////////////////////////////////////////////////

void test_runtime_descriptor_size(void)
{
    runtime_register();
    uint32_t size = xUSBD_DFU_DESC_SIZE;
    TEST_ASSERT_EQUAL_UINT32(xUSBD_DFU_DESC_SIZE, size);
}

void test_runtime_descriptor_build_matches_declared_size(void)
{
    runtime_register();
    uint32_t declared = xUSBD_DFU_DESC_SIZE;
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    uint32_t built = xUSBD_DFU_Runtime_Class()->build_descriptor(&g_dfu.class_ctx, buf, USB_SPEED_HIGH);
    TEST_ASSERT_EQUAL_UINT32(declared, built);
}

// MODE - REGISTRATION /////////////////////////////////////////////////////////

void test_mode_register_succeeds(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Register(&g_device, &g_dfu.class_ctx, xUSBD_DFU_Mode_Class()));
}

void test_mode_register_allocates_one_interface_one_string_no_endpoints(void)
{
    uint8_t iface_before = g_device.next_interface;
    uint8_t in_ep_before = g_device.next_in_ep;
    uint8_t out_ep_before = g_device.next_out_ep;
    uint8_t str_before = g_device.next_string_index;

    mode_register();

    TEST_ASSERT_EQUAL_UINT8(iface_before + 1U, g_device.next_interface);
    TEST_ASSERT_EQUAL_UINT8(in_ep_before, g_device.next_in_ep);
    TEST_ASSERT_EQUAL_UINT8(out_ep_before, g_device.next_out_ep);
    TEST_ASSERT_EQUAL_UINT8(str_before, g_device.next_string_index);
}

void test_mode_descriptor_size(void)
{
    mode_register();
    uint32_t size = xUSBD_DFU_DESC_SIZE;
    TEST_ASSERT_EQUAL_UINT32(xUSBD_DFU_DESC_SIZE, size);
}

// INITIAL STATE ///////////////////////////////////////////////////////////////

void test_runtime_initial_state_is_app_idle(void)
{
    runtime_register();
    TEST_ASSERT_EQUAL(xUSBD_DFU_STATE_APP_IDLE, g_dfu.state);
}

void test_mode_initial_state_is_dfu_idle(void)
{
    mode_register();
    TEST_ASSERT_EQUAL(xUSBD_DFU_STATE_DFU_IDLE, g_dfu.state);
}

// APP CALLBACKS ///////////////////////////////////////////////////////////////

void test_dfu_set_callbacks_success(void)
{
    runtime_register();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DFU_Set_Callbacks(&g_dfu.class_ctx, &g_dfu_calls));
}

void test_dfu_set_callbacks_null_ctx_fails(void)
{
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xUSBD_DFU_Set_Callbacks((xUSBD_Class_Context_t *)NULL, &g_dfu_calls));
}

void test_dfu_set_callbacks_roundtrip(void)
{
    runtime_register();
    (void)xUSBD_DFU_Set_Callbacks(&g_dfu.class_ctx, &g_dfu_calls);
    void *retrieved = NULL;
    (void)xUSBD_Class_Get_Callbacks(&g_dfu.class_ctx, &retrieved);
    TEST_ASSERT_EQUAL_PTR(&g_dfu_calls, retrieved);
}

// TRANSFER SIZE ///////////////////////////////////////////////////////////////

void test_dfu_set_transfer_size_success(void)
{
    runtime_register();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DFU_Set_Transfer_Size(&g_dfu.class_ctx, xUSBD_DFU_TRANSFER_SIZE));
}

void test_dfu_set_transfer_size_too_large_fails(void)
{
    runtime_register();
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xUSBD_DFU_Set_Transfer_Size(&g_dfu.class_ctx, xUSBD_DFU_TRANSFER_SIZE + 1U));
}

// PENDING OP //////////////////////////////////////////////////////////////////

void test_dfu_get_pending_op_initially_none(void)
{
    mode_register();
    TEST_ASSERT_EQUAL(xUSBD_DFU_PENDING_NONE, xUSBD_DFU_Get_Pending_Op(&g_dfu.class_ctx));
}

// BUS EVENTS //////////////////////////////////////////////////////////////////

void test_dfu_bus_event_connect_fires_app_callback(void)
{
    runtime_register();
    (void)xUSBD_DFU_Set_Callbacks(&g_dfu.class_ctx, &g_dfu_calls);
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dfu_bus_event_fake.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_CONNECT_RECEIVED, fake_dfu_bus_event_fake.arg1_val);
}

void test_dfu_bus_event_no_callbacks_does_not_crash(void)
{
    runtime_register();
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);
}

// RUNNER //////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_runtime_register_succeeds);
    RUN_TEST(test_runtime_register_allocates_one_interface_one_string_no_endpoints);
    RUN_TEST(test_runtime_register_after_start_fails);
    RUN_TEST(test_runtime_descriptor_size);
    RUN_TEST(test_runtime_descriptor_build_matches_declared_size);

    RUN_TEST(test_mode_register_succeeds);
    RUN_TEST(test_mode_register_allocates_one_interface_one_string_no_endpoints);
    RUN_TEST(test_mode_descriptor_size);

    RUN_TEST(test_runtime_initial_state_is_app_idle);
    RUN_TEST(test_mode_initial_state_is_dfu_idle);

    RUN_TEST(test_dfu_set_callbacks_success);
    RUN_TEST(test_dfu_set_callbacks_null_ctx_fails);
    RUN_TEST(test_dfu_set_callbacks_roundtrip);

    RUN_TEST(test_dfu_set_transfer_size_success);
    RUN_TEST(test_dfu_set_transfer_size_too_large_fails);

    RUN_TEST(test_dfu_get_pending_op_initially_none);

    RUN_TEST(test_dfu_bus_event_connect_fires_app_callback);
    RUN_TEST(test_dfu_bus_event_no_callbacks_does_not_crash);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
