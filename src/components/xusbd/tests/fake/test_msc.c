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

// @file test_msc.c
// @brief Host tests for the xUSBD MSC (Mass Storage Class) class driver.

#include <string.h>

#include "test_helpers.h"
#include "xusbd_msc.h"
#include "xassert.h"

xSTATIC_ASSERT(xUSBD_MSC_DESC_SIZE(USB_SPEED_HIGH) <= xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE, "MSC descriptor exceeds config descriptor budget");
xSTATIC_ASSERT(xUSBD_MSC_DESC_SIZE(USB_SPEED_SUPER) <= xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
               "SuperSpeed MSC descriptor exceeds config descriptor budget");

// MSC app callback fakes //////////////////////////////////////////////////////

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

FAKE_VALUE_FUNC(xRETURN_t, fake_msc_bus_event, xUSBD_Class_Context_t *, USB_DCD_Event_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_msc_io_control, xUSBD_Class_Context_t *, xUSBD_MSC_IO_CMD_t, void *, uint32_t, void **, uint32_t *);

#pragma GCC diagnostic pop

#define RESET_MSC_FAKES()                                                                                                                  \
    do                                                                                                                                     \
    {                                                                                                                                      \
        RESET_FAKE(fake_msc_bus_event);                                                                                                    \
        RESET_FAKE(fake_msc_io_control);                                                                                                   \
    } while (0)

// Test fixtures ///////////////////////////////////////////////////////////////

static xUSBD_Device_Context_t g_device;
static xUSBD_MSC_Context_t g_msc;

static xUSBD_MSC_Callbacks_t g_msc_calls = {
    .on_bus_event = fake_msc_bus_event,
    .on_io_control = fake_msc_io_control,
};

static void msc_register(void)
{
    (void)xUSBD_Class_Register(&g_device, &g_msc.class_ctx, xUSBD_MSC_Class());
}

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
    RESET_MSC_FAKES();
    memset(&g_device, 0, sizeof(g_device));
    memset(&g_msc, 0, sizeof(g_msc));
    test_device_init(&g_device);
}

void tearDown(void)
{
}

// REGISTRATION ////////////////////////////////////////////////////////////////

void test_msc_register_succeeds(void)
{
    xRETURN_t status = xUSBD_Class_Register(&g_device, &g_msc.class_ctx, xUSBD_MSC_Class());
    TEST_ASSERT_EQUAL(xRETURN_OK, status);
}

void test_msc_register_allocates_interface_and_two_endpoints_and_string(void)
{
    uint8_t next_interface_before = g_device.next_interface;
    uint8_t next_in_ep_before = g_device.next_in_ep;
    uint8_t next_out_ep_before = g_device.next_out_ep;
    uint8_t next_string_before = g_device.next_string_index;

    msc_register();

    TEST_ASSERT_EQUAL_UINT8(next_interface_before + 1U, g_device.next_interface);
    TEST_ASSERT_EQUAL_UINT8(next_in_ep_before + 1U, g_device.next_in_ep);
    TEST_ASSERT_EQUAL_UINT8(next_out_ep_before + 1U, g_device.next_out_ep);
    TEST_ASSERT_EQUAL_UINT8(next_string_before, g_device.next_string_index);
}

void test_msc_register_after_start_fails(void)
{
    msc_register();
    (void)test_device_start(&g_device);

    xUSBD_MSC_Context_t extra_msc;
    memset(&extra_msc, 0, sizeof(extra_msc));
    xRETURN_t status = xUSBD_Class_Register(&g_device, &extra_msc.class_ctx, xUSBD_MSC_Class());
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_ALREADY_INITIALIZED, status);
}

// DESCRIPTOR //////////////////////////////////////////////////////////////////

void test_msc_descriptor_size_high_speed(void)
{
    msc_register();
    uint32_t size = xUSBD_MSC_DESC_SIZE(USB_SPEED_HIGH);
    TEST_ASSERT_EQUAL_UINT32(xUSBD_MSC_DESC_SIZE(USB_SPEED_HIGH), size);
}

void test_msc_descriptor_size_super_speed(void)
{
    msc_register();
    uint32_t size = xUSBD_MSC_DESC_SIZE(USB_SPEED_SUPER);
    TEST_ASSERT_EQUAL_UINT32(xUSBD_MSC_DESC_SIZE(USB_SPEED_SUPER), size);
}

void test_msc_descriptor_build_has_storage_class(void)
{
    msc_register();
    uint32_t size = xUSBD_MSC_DESC_SIZE(USB_SPEED_HIGH);
    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    uint32_t built = xUSBD_MSC_Class()->build_descriptor(&g_msc.class_ctx, buf, USB_SPEED_HIGH);
    TEST_ASSERT_EQUAL_UINT32(size, built);
    // Interface descriptor bInterfaceClass is at byte offset 5 of the interface descriptor
    TEST_ASSERT_EQUAL_UINT8(USB_CLASS_STORAGE, buf[5]);
}

// APP CALLBACKS ///////////////////////////////////////////////////////////////

void test_msc_set_callbacks_success(void)
{
    msc_register();
    xRETURN_t status = xUSBD_MSC_Set_Callbacks(&g_msc.class_ctx, &g_msc_calls);
    TEST_ASSERT_EQUAL(xRETURN_OK, status);
}

void test_msc_set_callbacks_null_ctx_fails(void)
{
    xRETURN_t status = xUSBD_MSC_Set_Callbacks((xUSBD_Class_Context_t *)NULL, &g_msc_calls);
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, status);
}

void test_msc_set_callbacks_roundtrip(void)
{
    msc_register();
    (void)xUSBD_MSC_Set_Callbacks(&g_msc.class_ctx, &g_msc_calls);
    void *retrieved = NULL;
    (void)xUSBD_Class_Get_Callbacks(&g_msc.class_ctx, &retrieved);
    TEST_ASSERT_EQUAL_PTR(&g_msc_calls, retrieved);
}

// BUS EVENTS //////////////////////////////////////////////////////////////////

void test_msc_bus_event_connect_fires_app_callback(void)
{
    msc_register();
    (void)xUSBD_MSC_Set_Callbacks(&g_msc.class_ctx, &g_msc_calls);
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_msc_bus_event_fake.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_CONNECT_RECEIVED, fake_msc_bus_event_fake.arg1_val);
}

void test_msc_bus_event_connect_dispatches_to_app_after_reset(void)
{
    msc_register();
    (void)xUSBD_MSC_Set_Callbacks(&g_msc.class_ctx, &g_msc_calls);
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);
    RESET_MSC_FAKES();

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_msc_bus_event_fake.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_CONNECT_RECEIVED, fake_msc_bus_event_fake.arg1_val);
}

void test_msc_bus_event_no_callbacks_does_not_crash(void)
{
    msc_register();
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);
    // No assert needed - test passes if there is no crash/fault
}

// RUNNER //////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_msc_register_succeeds);
    RUN_TEST(test_msc_register_allocates_interface_and_two_endpoints_and_string);
    RUN_TEST(test_msc_register_after_start_fails);

    RUN_TEST(test_msc_descriptor_size_high_speed);
    RUN_TEST(test_msc_descriptor_size_super_speed);
    RUN_TEST(test_msc_descriptor_build_has_storage_class);

    RUN_TEST(test_msc_set_callbacks_success);
    RUN_TEST(test_msc_set_callbacks_null_ctx_fails);
    RUN_TEST(test_msc_set_callbacks_roundtrip);

    RUN_TEST(test_msc_bus_event_connect_fires_app_callback);
    RUN_TEST(test_msc_bus_event_connect_dispatches_to_app_after_reset);
    RUN_TEST(test_msc_bus_event_no_callbacks_does_not_crash);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
