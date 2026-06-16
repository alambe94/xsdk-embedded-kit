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

// @file test_xusbh_port.c
// @brief Host tests for root-port connection, reset, and disconnect state handling.

#include <string.h>

#include "unity.h"

#include "xusbh_core.h"
#include "test_xusbh_helpers.h"

static xUSBH_Context_t g_host;

static void emit_port_event(xUSBH_HCD_Port_Event_t port_event)
{
    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_PORT,
        .port = 0U,
        .port_event = port_event,
    };

    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &event);
}

static void drive_root_port_to_enumerating(void)
{
    emit_port_event(xUSBH_HCD_PORT_EVENT_CONNECTED);
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_CONNECTED, g_host.root_ports[0].state);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_CONNECTED, g_host.root_ports[0].state);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_POWERED, g_host.root_ports[0].state);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_RESETTING, g_host.root_ports[0].state);

    emit_port_event(xUSBH_HCD_PORT_EVENT_RESET_COMPLETE);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ENABLED, g_host.root_ports[0].state);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ENUMERATING, g_host.root_ports[0].state);
}

void setUp(void)
{
    (void)memset(&g_host, 0, sizeof(g_host));
    reset_fake_hcd();
}

void tearDown(void)
{
}

void test_root_port_state_api_reports_defaults_and_rejects_bad_args(void)
{
    xUSBH_Root_Port_State_t state = xUSBH_ROOT_PORT_ERROR;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Root_Port_Get_State(NULL, 0U, &state));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Root_Port_Get_State(&g_host, 0U, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NOT_INITIALIZED, xUSBH_Root_Port_Get_State(&g_host, 0U, &state));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&g_host, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT, xUSBH_Root_Port_Get_State(&g_host, 1U, &state));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Root_Port_Get_State(&g_host, 0U, &state));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_DISCONNECTED, state);
}

void test_connect_sequence_debounces_powers_resets_and_enters_enumerating(void)
{
    init_and_start_host(&g_host);

    emit_port_event(xUSBH_HCD_PORT_EVENT_CONNECTED);
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_CONNECTED, g_host.root_ports[0].state);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_CONNECTED, g_host.root_ports[0].state);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.get_port_status_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.port_power_count);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_POWERED, g_host.root_ports[0].state);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.port_power_count);
    TEST_ASSERT_TRUE(g_fake_hcd.last_port_power_enable);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_RESETTING, g_host.root_ports[0].state);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.port_reset_count);

    emit_port_event(xUSBH_HCD_PORT_EVENT_RESET_COMPLETE);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ENABLED, g_host.root_ports[0].state);
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, g_host.root_ports[0].speed);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ENUMERATING, g_host.root_ports[0].state);
    TEST_ASSERT_TRUE(g_host.root_ports[0].has_device);
    TEST_ASSERT_TRUE(g_host.devices[0].is_allocated);
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, g_host.devices[0].speed);
}

void test_unstable_connect_returns_to_disconnected_without_power(void)
{
    init_and_start_host(&g_host);
    g_fake_hcd.port_status.is_connected = false;

    emit_port_event(xUSBH_HCD_PORT_EVENT_CONNECTED);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_DISCONNECTED, g_host.root_ports[0].state);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.port_power_count);
}

void test_disconnect_during_enumeration_cancels_transfer_and_returns_to_disconnected(void)
{
    xUSBH_Transfer_t *transfer = NULL;

    init_and_start_host(&g_host);
    drive_root_port_to_enumerating();

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Allocate(&g_host, &transfer));
    transfer->device_address = 0U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Submit(&g_host, transfer));
    TEST_ASSERT_TRUE(transfer->is_submitted);

    emit_port_event(xUSBH_HCD_PORT_EVENT_DISCONNECTED);
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_DISCONNECTED, g_host.root_ports[0].state);
    TEST_ASSERT_FALSE(g_host.root_ports[0].has_device);
    TEST_ASSERT_FALSE(g_host.devices[0].is_allocated);
    TEST_ASSERT_FALSE(transfer->is_allocated);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.cancel_transfer_count);
}

void test_port_power_failure_moves_port_to_error(void)
{
    init_and_start_host(&g_host);
    g_fake_hcd.port_power_return = xRETURN_xERR_xUSBH_INVALID_STATE;

    emit_port_event(xUSBH_HCD_PORT_EVENT_CONNECTED);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_STATE, xUSBH_Process(&g_host));
    TEST_ASSERT_EQUAL(xUSBH_ROOT_PORT_ERROR, g_host.root_ports[0].state);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_root_port_state_api_reports_defaults_and_rejects_bad_args);
    RUN_TEST(test_connect_sequence_debounces_powers_resets_and_enters_enumerating);
    RUN_TEST(test_unstable_connect_returns_to_disconnected_without_power);
    RUN_TEST(test_disconnect_during_enumeration_cancels_transfer_and_returns_to_disconnected);
    RUN_TEST(test_port_power_failure_moves_port_to_error);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
