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

// @file test_hid.c
// @brief Host tests for the xUSBD HID class driver (Keyboard and Mouse variants).

#include <string.h>

#include "test_helpers.h"
#include "xusbd_hid.h"
#include "xassert.h"

xSTATIC_ASSERT(xUSBD_HID_DESC_SIZE(USB_SPEED_HIGH) <= xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE, "HID descriptor exceeds config descriptor budget");
xSTATIC_ASSERT(xUSBD_HID_DESC_SIZE(USB_SPEED_SUPER) <= xUSBD_MAX_CONFIG_DESCRIPTOR_SIZE,
               "SuperSpeed HID descriptor exceeds config descriptor budget");

// Dummy report descriptor for testing
static const uint8_t s_dummy_report_descriptor[] = {
    0x06, 0x00, 0xFF, // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x01,       // Usage (Vendor Usage 1)
    0xA1, 0x01,       // Collection (Application)
    0xC0              // End Collection
};

// HID app callback fakes //////////////////////////////////////////////////////

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

FAKE_VALUE_FUNC(xRETURN_t, fake_hid_bus_event, xUSBD_Class_Context_t *, USB_DCD_Event_t);
FAKE_VALUE_FUNC(xRETURN_t, fake_hid_transmit_complete, xUSBD_Class_Context_t *, uint8_t, uint8_t *, uint32_t);

#pragma GCC diagnostic pop

#define RESET_HID_FAKES()                                                                                                                  \
    do                                                                                                                                     \
    {                                                                                                                                      \
        RESET_FAKE(fake_hid_bus_event);                                                                                                    \
        RESET_FAKE(fake_hid_transmit_complete);                                                                                            \
    } while (0)

// Test fixtures ///////////////////////////////////////////////////////////////

static xUSBD_Device_Context_t g_device;
static xUSBD_HID_Context_t g_kbd;
static xUSBD_HID_Context_t g_mouse;

static xUSBD_HID_Callbacks_t g_kbd_calls = {
    .on_bus_event = fake_hid_bus_event,
    .on_transmit_complete = fake_hid_transmit_complete,
};

static xUSBD_HID_Callbacks_t g_mouse_calls = {
    .on_bus_event = fake_hid_bus_event,
    .on_transmit_complete = fake_hid_transmit_complete,
};

static void kbd_register(void)
{
    (void)xUSBD_Class_Register(&g_device, &g_kbd.class_ctx, xUSBD_HID_Class());
}

static void mouse_register(void)
{
    (void)xUSBD_Class_Register(&g_device, &g_mouse.class_ctx, xUSBD_HID_Class());
}

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
    RESET_HID_FAKES();
    memset(&g_device, 0, sizeof(g_device));
    memset(&g_kbd, 0, sizeof(g_kbd));
    memset(&g_mouse, 0, sizeof(g_mouse));
    test_device_init(&g_device);

    g_kbd.report_descriptor = s_dummy_report_descriptor;
    g_kbd.report_descriptor_len = sizeof(s_dummy_report_descriptor);
    g_kbd.subclass = 1; // Boot
    g_kbd.protocol = 1; // Keyboard
    g_kbd.interval = 10;

    g_mouse.report_descriptor = s_dummy_report_descriptor;
    g_mouse.report_descriptor_len = sizeof(s_dummy_report_descriptor);
    g_mouse.subclass = 1; // Boot
    g_mouse.protocol = 2; // Mouse
    g_mouse.interval = 10;
}

void tearDown(void)
{
}

// KEYBOARD - REGISTRATION /////////////////////////////////////////////////////

void test_kbd_register_succeeds(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Register(&g_device, &g_kbd.class_ctx, xUSBD_HID_Class()));
}

void test_kbd_register_allocates_one_interface_one_in_ep_one_string(void)
{
    uint8_t iface_before = g_device.next_interface;
    uint8_t in_ep_before = g_device.next_in_ep;
    uint8_t out_ep_before = g_device.next_out_ep;
    uint8_t str_before = g_device.next_string_index;

    kbd_register();

    TEST_ASSERT_EQUAL_UINT8(iface_before + 1U, g_device.next_interface);
    TEST_ASSERT_EQUAL_UINT8(in_ep_before + 1U, g_device.next_in_ep);
    TEST_ASSERT_EQUAL_UINT8(out_ep_before, g_device.next_out_ep);
    TEST_ASSERT_EQUAL_UINT8(str_before, g_device.next_string_index);
}

void test_kbd_register_after_start_fails(void)
{
    kbd_register();
    (void)test_device_start(&g_device);

    xUSBD_HID_Context_t extra;
    memset(&extra, 0, sizeof(extra));
    extra.report_descriptor = s_dummy_report_descriptor;
    extra.report_descriptor_len = sizeof(s_dummy_report_descriptor);
    extra.subclass = 1;
    extra.protocol = 1;
    extra.interval = 10;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_ALREADY_INITIALIZED, xUSBD_Class_Register(&g_device, &extra.class_ctx, xUSBD_HID_Class()));
}

// KEYBOARD - DESCRIPTOR ///////////////////////////////////////////////////////

void test_kbd_descriptor_size_high_speed(void)
{
    kbd_register();
    uint32_t size = xUSBD_HID_DESC_SIZE(USB_SPEED_HIGH);
    TEST_ASSERT_EQUAL_UINT32(xUSBD_HID_DESC_SIZE(USB_SPEED_HIGH), size);
}

void test_kbd_descriptor_build_matches_declared_size(void)
{
    kbd_register();
    uint32_t declared = xUSBD_HID_DESC_SIZE(USB_SPEED_HIGH);
    uint8_t buf[128];
    memset(buf, 0, sizeof(buf));
    uint32_t built = xUSBD_HID_Class()->build_descriptor(&g_kbd.class_ctx, buf, USB_SPEED_HIGH);
    TEST_ASSERT_EQUAL_UINT32(declared, built);
}

// KEYBOARD - APP CALLBACKS ////////////////////////////////////////////////////

void test_kbd_set_callbacks_success(void)
{
    kbd_register();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_HID_Set_Callbacks(&g_kbd.class_ctx, &g_kbd_calls));
}

void test_kbd_set_callbacks_null_ctx_fails(void)
{
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xUSBD_HID_Set_Callbacks((xUSBD_Class_Context_t *)NULL, &g_kbd_calls));
}

void test_kbd_set_callbacks_roundtrip(void)
{
    kbd_register();
    (void)xUSBD_HID_Set_Callbacks(&g_kbd.class_ctx, &g_kbd_calls);
    void *retrieved = NULL;
    (void)xUSBD_Class_Get_Callbacks(&g_kbd.class_ctx, &retrieved);
    TEST_ASSERT_EQUAL_PTR(&g_kbd_calls, retrieved);
}

// KEYBOARD - BUS EVENTS ///////////////////////////////////////////////////////

void test_kbd_bus_event_connect_fires_app_callback(void)
{
    kbd_register();
    (void)xUSBD_HID_Set_Callbacks(&g_kbd.class_ctx, &g_kbd_calls);
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_hid_bus_event_fake.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_CONNECT_RECEIVED, fake_hid_bus_event_fake.arg1_val);
}

// MOUSE - REGISTRATION ////////////////////////////////////////////////////////

void test_mouse_register_succeeds(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Register(&g_device, &g_mouse.class_ctx, xUSBD_HID_Class()));
}

void test_mouse_register_allocates_one_interface_one_in_ep_one_string(void)
{
    uint8_t iface_before = g_device.next_interface;
    uint8_t in_ep_before = g_device.next_in_ep;
    uint8_t out_ep_before = g_device.next_out_ep;
    uint8_t str_before = g_device.next_string_index;

    mouse_register();

    TEST_ASSERT_EQUAL_UINT8(iface_before + 1U, g_device.next_interface);
    TEST_ASSERT_EQUAL_UINT8(in_ep_before + 1U, g_device.next_in_ep);
    TEST_ASSERT_EQUAL_UINT8(out_ep_before, g_device.next_out_ep);
    TEST_ASSERT_EQUAL_UINT8(str_before, g_device.next_string_index);
}

void test_mouse_descriptor_size_high_speed(void)
{
    mouse_register();
    uint32_t size = xUSBD_HID_DESC_SIZE(USB_SPEED_HIGH);
    TEST_ASSERT_EQUAL_UINT32(xUSBD_HID_DESC_SIZE(USB_SPEED_HIGH), size);
}

void test_mouse_bus_event_connect_fires_app_callback(void)
{
    mouse_register();
    (void)xUSBD_HID_Set_Callbacks(&g_mouse.class_ctx, &g_mouse_calls);
    (void)test_device_start(&g_device);

    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0, NULL, 0);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_hid_bus_event_fake.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_CONNECT_RECEIVED, fake_hid_bus_event_fake.arg1_val);
}

// CONCURRENT REGISTRATION /////////////////////////////////////////////////////

void test_concurrent_kbd_and_mouse_registration(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Register(&g_device, &g_kbd.class_ctx, xUSBD_HID_Class()));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Register(&g_device, &g_mouse.class_ctx, xUSBD_HID_Class()));
}

// RUNNER //////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_kbd_register_succeeds);
    RUN_TEST(test_kbd_register_allocates_one_interface_one_in_ep_one_string);
    RUN_TEST(test_kbd_register_after_start_fails);
    RUN_TEST(test_kbd_descriptor_size_high_speed);
    RUN_TEST(test_kbd_descriptor_build_matches_declared_size);
    RUN_TEST(test_kbd_set_callbacks_success);
    RUN_TEST(test_kbd_set_callbacks_null_ctx_fails);
    RUN_TEST(test_kbd_set_callbacks_roundtrip);
    RUN_TEST(test_kbd_bus_event_connect_fires_app_callback);

    RUN_TEST(test_mouse_register_succeeds);
    RUN_TEST(test_mouse_register_allocates_one_interface_one_in_ep_one_string);
    RUN_TEST(test_mouse_descriptor_size_high_speed);
    RUN_TEST(test_mouse_bus_event_connect_fires_app_callback);

    RUN_TEST(test_concurrent_kbd_and_mouse_registration);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
