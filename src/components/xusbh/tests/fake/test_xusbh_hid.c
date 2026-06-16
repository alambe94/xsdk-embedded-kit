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

// @file test_xusbh_hid.c
// @brief Host tests for the xUSBH HID boot keyboard and boot mouse class.

#include <string.h>

#include "unity.h"

#include "xusbh_hid.h"
#include "test_xusbh_helpers.h"

typedef struct HID_App_Context_t
{
    uint32_t keyboard_report_count;
    uint32_t mouse_report_count;
    xUSBH_HID_Keyboard_Report_t last_keyboard_report;
    xUSBH_HID_Mouse_Report_t last_mouse_report;
} HID_App_Context_t;

static xUSBH_Context_t g_host;
static xUSBH_HID_Context_t g_hid;
static HID_App_Context_t g_app;

static void keyboard_report_callback(void *user_ctx, const xUSBH_HID_Keyboard_Report_t *report)
{
    HID_App_Context_t *app = (HID_App_Context_t *)user_ctx;

    app->keyboard_report_count++;
    app->last_keyboard_report = *report;
}

static void mouse_report_callback(void *user_ctx, const xUSBH_HID_Mouse_Report_t *report)
{
    HID_App_Context_t *app = (HID_App_Context_t *)user_ctx;

    app->mouse_report_count++;
    app->last_mouse_report = *report;
}

static void host_init_register_hid_and_start(void)
{
    xUSBH_HID_Callbacks_t callbacks = {
        .keyboard_report = keyboard_report_callback,
        .mouse_report = mouse_report_callback,
    };
    xUSBH_Class_Register_Config_t class_config = {
        .driver = xUSBH_HID_Class(),
        .class_ctx = &g_hid,
    };
    xUSBH_Start_Config_t start_config = {
        .hcd_ops = &fake_hcd_ops,
        .hcd_ctx = &g_fake_hcd,
    };

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&g_host, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HID_Init(&g_hid, &g_host, &callbacks, &g_app));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Register_Class(&g_host, &class_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&g_host, &start_config));
}

static void allocate_boot_hid_interface(uint8_t protocol,
                                        uint8_t endpoint_address,
                                        xUSBH_Device_Context_t **device,
                                        xUSBH_Interface_Context_t **interface,
                                        xUSBH_Endpoint_Context_t **endpoint)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Allocate(&g_host, 0U, device));
    (*device)->address = 3U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Interface_Allocate(&g_host, *device, interface));
    (*interface)->class_code = USB_CLASS_HID;
    (*interface)->subclass = xUSBH_HID_BOOT_SUBCLASS;
    (*interface)->protocol = protocol;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, *interface, endpoint));
    (*endpoint)->endpoint_address = endpoint_address;
    (*endpoint)->endpoint_type = USB_ENDP_TYPE_INTR;
    (*endpoint)->is_in = true;
    (*endpoint)->max_packet_size = 8U;
    (*endpoint)->interval = 10U;
    (*interface)->endpoint_count++;
}

static void complete_transfer(xUSBH_Transfer_t *transfer, const uint8_t *data, uint32_t length)
{
    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_TRANSFER,
        .transfer_event = xUSBH_HCD_TRANSFER_EVENT_COMPLETE,
        .transfer = transfer,
    };

    (void)memcpy(transfer->data, data, length);
    transfer->actual_length = length;
    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &event);
}

void setUp(void)
{
    (void)memset(&g_host, 0, sizeof(g_host));
    (void)memset(&g_hid, 0, sizeof(g_hid));
    (void)memset(&g_app, 0, sizeof(g_app));
    reset_fake_hcd();
}

void tearDown(void)
{
}

void test_xusbh_hid_matches_boot_keyboard_and_resubmits_interrupt_in_report(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;
    static const uint8_t keyboard_report[xUSBH_HID_KEYBOARD_REPORT_SIZE] = {
        0x02U, 0x00U, 0x04U, 0x05U, 0x00U, 0x00U, 0x00U, 0x00U,
    };

    host_init_register_hid_and_start();
    allocate_boot_hid_interface(xUSBH_HID_PROTOCOL_KEYBOARD, 0x81U, &device, &interface, &endpoint);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_EQUAL_PTR(xUSBH_HID_Class(), interface->class_driver);
    TEST_ASSERT_TRUE(g_hid.instances[0].is_allocated);
    TEST_ASSERT_EQUAL(xUSBH_HID_REPORT_TYPE_KEYBOARD, g_hid.instances[0].report_type);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT8(3U, g_fake_hcd.last_transfer->device_address);
    TEST_ASSERT_EQUAL_UINT8(0x81U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL_UINT32(xUSBH_HID_KEYBOARD_REPORT_SIZE, g_fake_hcd.last_transfer->length);

    complete_transfer(g_fake_hcd.last_transfer, keyboard_report, sizeof(keyboard_report));
    TEST_ASSERT_EQUAL_UINT32(1U, g_app.keyboard_report_count);
    TEST_ASSERT_EQUAL_UINT8(0x02U, g_app.last_keyboard_report.modifiers);
    TEST_ASSERT_EQUAL_UINT8(0x04U, g_app.last_keyboard_report.keys[0]);
    TEST_ASSERT_EQUAL_UINT8(0x05U, g_app.last_keyboard_report.keys[1]);
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_TRUE(g_fake_hcd.last_transfer->is_submitted);
}

void test_xusbh_hid_matches_boot_mouse_and_reports_motion(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;
    static const uint8_t mouse_report[xUSBH_HID_MOUSE_REPORT_SIZE] = {
        0x01U,
        0x02U,
        0xFEU,
        0x01U,
    };

    host_init_register_hid_and_start();
    allocate_boot_hid_interface(xUSBH_HID_PROTOCOL_MOUSE, 0x82U, &device, &interface, &endpoint);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_EQUAL(xUSBH_HID_REPORT_TYPE_MOUSE, g_hid.instances[0].report_type);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT32(xUSBH_HID_MOUSE_REPORT_SIZE, g_fake_hcd.last_transfer->length);

    complete_transfer(g_fake_hcd.last_transfer, mouse_report, sizeof(mouse_report));
    TEST_ASSERT_EQUAL_UINT32(1U, g_app.mouse_report_count);
    TEST_ASSERT_EQUAL_UINT8(0x01U, g_app.last_mouse_report.buttons);
    TEST_ASSERT_EQUAL_INT8(2, g_app.last_mouse_report.x);
    TEST_ASSERT_EQUAL_INT8(-2, g_app.last_mouse_report.y);
    TEST_ASSERT_EQUAL_INT8(1, g_app.last_mouse_report.wheel);
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_hcd.submit_transfer_count);
}

void test_xusbh_hid_ignores_non_boot_hid_protocol(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;

    host_init_register_hid_and_start();
    allocate_boot_hid_interface(0U, 0x81U, &device, &interface, &endpoint);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_NULL(interface->class_driver);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.submit_transfer_count);
}

void test_xusbh_hid_stop_releases_interrupt_transfer(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;

    host_init_register_hid_and_start();
    allocate_boot_hid_interface(xUSBH_HID_PROTOCOL_KEYBOARD, 0x81U, &device, &interface, &endpoint);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_TRUE(g_hid.instances[0].is_allocated);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Release(&g_host, device));
    TEST_ASSERT_FALSE(g_hid.instances[0].is_allocated);
    TEST_ASSERT_FALSE(g_host.transfers[0].is_allocated);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.cancel_transfer_count);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xusbh_hid_matches_boot_keyboard_and_resubmits_interrupt_in_report);
    RUN_TEST(test_xusbh_hid_matches_boot_mouse_and_reports_motion);
    RUN_TEST(test_xusbh_hid_ignores_non_boot_hid_protocol);
    RUN_TEST(test_xusbh_hid_stop_releases_interrupt_transfer);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
