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

// @file test_bus_events.c
// @brief Host tests for bus-event propagation to class drivers via
// xUSBD_Class_Bus_Event_Process. Each test fires one DCD event through
// the registered callback and verifies the class driver's bus_event
// handler is called with the correct event value.

#include "unity.h"
#include "test_helpers.h"

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
}
void tearDown(void)
{
}

// BUS EVENT PROBE /////////////////////////////////////////////////////////////

typedef struct
{
    uint32_t call_count;
    USB_DCD_Event_t last_event;
    USB_Speed_t last_descriptor_speed;
} Bus_Event_Probe_t;

static uint32_t bus_event_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    void *ctx = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &ctx) == xRETURN_OK && ctx != NULL)
    {
        Bus_Event_Probe_t *probe = (Bus_Event_Probe_t *)ctx;
        probe->last_descriptor_speed = speed;
    }

    return normal_build_descriptor(class_ctx, buffer, speed);
}

static xRETURN_t bus_event_callback(xUSBD_Class_Context_t *class_ctx, USB_DCD_Event_t event)
{
    void *ctx = NULL;
    if (xUSBD_Class_Get_App_Context(class_ctx, &ctx) != xRETURN_OK || ctx == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }
    Bus_Event_Probe_t *probe = (Bus_Event_Probe_t *)ctx;
    probe->call_count++;
    probe->last_event = event;
    return xRETURN_OK;
}

static xUSBD_Class_Driver_t bus_event_driver = {
    .init_instance = one_interface_init,
    .build_descriptor = bus_event_build_descriptor,
    .bus_event = bus_event_callback,
};

// SETUP HELPER ////////////////////////////////////////////////////////////////

static void
prepare_started_device_with_probe(xUSBD_Device_Context_t *device_ctx, xUSBD_Class_Context_t *class_ctx, Bus_Event_Probe_t *probe)
{
    memset(class_ctx, 0, sizeof(*class_ctx));
    memset(probe, 0, sizeof(*probe));
    (void)xUSBD_Class_Set_App_Context(class_ctx, probe);
    test_device_init(device_ctx);
    (void)test_class_register(device_ctx, class_ctx, &bus_event_driver, NULL);
    (void)test_device_start(device_ctx);
}

// TESTS ///////////////////////////////////////////////////////////////////////

void test_bus_event_connect_propagates_to_class_driver(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Bus_Event_Probe_t probe;

    prepare_started_device_with_probe(&device_ctx, &class_ctx, &probe);

    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(1U, probe.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_CONNECT_RECEIVED, probe.last_event);
}

void test_bus_event_reset_propagates_to_class_driver(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Bus_Event_Probe_t probe;

    prepare_started_device_with_probe(&device_ctx, &class_ctx, &probe);

    dcd_fire_event(&device_ctx, USB_DCD_RESET_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(1U, probe.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_RESET_RECEIVED, probe.last_event);
}

void test_bus_event_suspend_propagates_to_class_driver(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Bus_Event_Probe_t probe;

    prepare_started_device_with_probe(&device_ctx, &class_ctx, &probe);

    dcd_fire_event(&device_ctx, USB_DCD_SUSPEND_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(1U, probe.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_SUSPEND_RECEIVED, probe.last_event);
}

void test_bus_event_resume_propagates_to_class_driver(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Bus_Event_Probe_t probe;

    prepare_started_device_with_probe(&device_ctx, &class_ctx, &probe);

    dcd_fire_event(&device_ctx, USB_DCD_RESUME_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(1U, probe.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_RESUME_RECEIVED, probe.last_event);
}

void test_bus_event_disconnect_propagates_to_class_driver(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Bus_Event_Probe_t probe;

    prepare_started_device_with_probe(&device_ctx, &class_ctx, &probe);

    dcd_fire_event(&device_ctx, USB_DCD_DISCONNECT_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(1U, probe.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_DISCONNECT_RECEIVED, probe.last_event);
}

void test_bus_event_speed_change_rebuilds_descriptor_and_propagates_to_class_driver(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Bus_Event_Probe_t probe;

    prepare_started_device_with_probe(&device_ctx, &class_ctx, &probe);

    fake_dcd_get_speed_fake.return_val = USB_SPEED_HIGH;
    dcd_fire_event(&device_ctx, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    probe.call_count = 0U;
    probe.last_event = USB_DCD_SETUP_RECEIVED;
    probe.last_descriptor_speed = USB_SPEED_LOW;
    fake_dcd_get_speed_fake.return_val = USB_SPEED_SUPER;
    dcd_fire_event(&device_ctx, USB_DCD_SPEED_CHANGE_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL_UINT32(1U, probe.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_SPEED_CHANGE_RECEIVED, probe.last_event);
    TEST_ASSERT_EQUAL(USB_SPEED_SUPER, probe.last_descriptor_speed);
    TEST_ASSERT_EQUAL(USB_SPEED_SUPER, device_ctx.speed);
}

void test_bus_event_link_state_change_updates_state_and_propagates_to_class_driver(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_ctx;
    Bus_Event_Probe_t probe;
    xUSBD_DCD_Link_State_Event_t link_event = {
        .link_state = USB_DCD_LINK_STATE_U2,
    };
    USB_DCD_Link_State_t link_state = USB_DCD_LINK_STATE_UNKNOWN;

    prepare_started_device_with_probe(&device_ctx, &class_ctx, &probe);

    RESET_FAKE(fake_dcd_ep_init);
    RESET_FAKE(fake_dcd_ep_receive);
    dcd_fire_event(&device_ctx, USB_DCD_LINK_STATE_CHANGE_RECEIVED, 0U, (uint8_t *)&link_event, sizeof(link_event));

    TEST_ASSERT_EQUAL_UINT32(1U, probe.call_count);
    TEST_ASSERT_EQUAL(USB_DCD_LINK_STATE_CHANGE_RECEIVED, probe.last_event);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Get_Link_State(&device_ctx, &link_state));
    TEST_ASSERT_EQUAL(USB_DCD_LINK_STATE_U2, link_state);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_init_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dcd_ep_receive_fake.call_count);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bus_event_connect_propagates_to_class_driver);
    RUN_TEST(test_bus_event_reset_propagates_to_class_driver);
    RUN_TEST(test_bus_event_suspend_propagates_to_class_driver);
    RUN_TEST(test_bus_event_resume_propagates_to_class_driver);
    RUN_TEST(test_bus_event_disconnect_propagates_to_class_driver);
    RUN_TEST(test_bus_event_speed_change_rebuilds_descriptor_and_propagates_to_class_driver);
    RUN_TEST(test_bus_event_link_state_change_updates_state_and_propagates_to_class_driver);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////
