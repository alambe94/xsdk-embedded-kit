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

// @file test_xusbh_transfer.c
// @brief Host tests for xUSBH static slots and transfer ownership.

#include <string.h>

#include "unity.h"

#include "xusbh_core.h"
#include "test_xusbh_helpers.h"

static xUSBH_Context_t g_host;

static void host_init_only(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&g_host, &valid_init_config));
}

static void host_init_and_start(void)
{
    host_init_only();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&g_host, &valid_start_config));
}

static void allocate_device_interface_endpoint(xUSBH_Device_Context_t **device,
                                               xUSBH_Interface_Context_t **interface,
                                               xUSBH_Endpoint_Context_t **endpoint)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Allocate(&g_host, 0U, device));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Interface_Allocate(&g_host, *device, interface));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, *interface, endpoint));
}

void setUp(void)
{
    (void)memset(&g_host, 0, sizeof(g_host));
    reset_fake_hcd();
}

void tearDown(void)
{
}

void test_xusbh_static_slots_start_free_and_reject_invalid_args(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;
    xUSBH_Transfer_t *transfer = NULL;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Device_Allocate(NULL, 0U, &device));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NOT_INITIALIZED, xUSBH_Device_Allocate(&g_host, 0U, &device));

    host_init_only();

    TEST_ASSERT_FALSE(g_host.devices[0].is_allocated);
    TEST_ASSERT_FALSE(g_host.interfaces[0].is_allocated);
    TEST_ASSERT_FALSE(g_host.endpoints[0].is_allocated);
    TEST_ASSERT_FALSE(g_host.transfers[0].is_allocated);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Device_Allocate(&g_host, 0U, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT, xUSBH_Device_Allocate(&g_host, 1U, &device));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Interface_Allocate(&g_host, NULL, &interface));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Endpoint_Allocate(&g_host, NULL, &endpoint));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Transfer_Allocate(&g_host, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Allocate(&g_host, &transfer));
    TEST_ASSERT_TRUE(transfer->is_allocated);
}

void test_xusbh_slot_exhaustion_is_explicit(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interfaces[xUSBH_MAX_INTERFACES];
    xUSBH_Endpoint_Context_t *endpoints[xUSBH_MAX_ENDPOINTS];
    xUSBH_Transfer_t *transfers[xUSBH_MAX_TRANSFERS];
    uint8_t i;

    host_init_only();

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Allocate(&g_host, 0U, &device));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED, xUSBH_Device_Allocate(&g_host, 0U, &device));

    for (i = 0U; i < xUSBH_MAX_INTERFACES; i++)
    {
        TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Interface_Allocate(&g_host, device, &interfaces[i]));
    }
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED, xUSBH_Interface_Allocate(&g_host, device, &interfaces[0]));

    for (i = 0U; i < xUSBH_MAX_ENDPOINTS; i++)
    {
        TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, interfaces[0], &endpoints[i]));
    }
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED, xUSBH_Endpoint_Allocate(&g_host, interfaces[0], &endpoints[0]));

    for (i = 0U; i < xUSBH_MAX_TRANSFERS; i++)
    {
        TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Allocate(&g_host, &transfers[i]));
    }
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_RESOURCE_EXHAUSTED, xUSBH_Transfer_Allocate(&g_host, &transfers[0]));
}

void test_xusbh_release_cascades_interface_and_endpoint_slots(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;

    host_init_only();
    allocate_device_interface_endpoint(&device, &interface, &endpoint);

    TEST_ASSERT_TRUE(device->is_allocated);
    TEST_ASSERT_TRUE(interface->is_allocated);
    TEST_ASSERT_TRUE(endpoint->is_allocated);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Release(&g_host, device));
    TEST_ASSERT_FALSE(device->is_allocated);
    TEST_ASSERT_FALSE(interface->is_allocated);
    TEST_ASSERT_FALSE(endpoint->is_allocated);
}

void test_xusbh_transfer_submit_cancel_and_release_use_hcd_ops(void)
{
    xUSBH_Transfer_t *transfer = NULL;

    host_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Allocate(&g_host, &transfer));

    transfer->device_address = 5U;
    transfer->endpoint_address = 0x81U;
    transfer->endpoint_type = USB_ENDP_TYPE_INTR;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Submit(&g_host, transfer));
    TEST_ASSERT_TRUE(transfer->is_submitted);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_PTR(transfer, g_fake_hcd.last_transfer);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_STATE, xUSBH_Transfer_Submit(&g_host, transfer));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Cancel(&g_host, transfer));
    TEST_ASSERT_FALSE(transfer->is_submitted);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.cancel_transfer_count);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_STATE, xUSBH_Transfer_Cancel(&g_host, transfer));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Release(&g_host, transfer));
    TEST_ASSERT_FALSE(transfer->is_allocated);
}

void test_xusbh_transfer_submit_propagates_hcd_failure(void)
{
    xUSBH_Transfer_t *transfer = NULL;

    host_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Allocate(&g_host, &transfer));
    g_fake_hcd.submit_transfer_return = xRETURN_xERR_xUSBH_INVALID_STATE;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_STATE, xUSBH_Transfer_Submit(&g_host, transfer));
    TEST_ASSERT_FALSE(transfer->is_submitted);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.submit_transfer_count);
}

void test_xusbh_transfer_completion_event_releases_submission_ownership(void)
{
    xUSBH_Transfer_t *transfer = NULL;
    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_TRANSFER,
        .transfer_event = xUSBH_HCD_TRANSFER_EVENT_COMPLETE,
    };

    host_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Allocate(&g_host, &transfer));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Submit(&g_host, transfer));
    TEST_ASSERT_TRUE(transfer->is_submitted);

    event.transfer = transfer;
    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &event);
    TEST_ASSERT_FALSE(transfer->is_submitted);
}

void test_xusbh_port_disconnect_cleanup_releases_owned_slots(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;
    xUSBH_Transfer_t *transfer = NULL;

    host_init_and_start();
    allocate_device_interface_endpoint(&device, &interface, &endpoint);
    device->address = 9U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Allocate(&g_host, &transfer));
    transfer->device_address = 9U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Transfer_Submit(&g_host, transfer));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Port_Disconnect_Cleanup(&g_host, 0U));

    TEST_ASSERT_FALSE(device->is_allocated);
    TEST_ASSERT_FALSE(interface->is_allocated);
    TEST_ASSERT_FALSE(endpoint->is_allocated);
    TEST_ASSERT_FALSE(transfer->is_allocated);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.cancel_transfer_count);
}

void test_xusbh_disconnect_event_runs_port_cleanup(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_PORT,
        .port = 0U,
        .port_event = xUSBH_HCD_PORT_EVENT_DISCONNECTED,
    };

    host_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Allocate(&g_host, 0U, &device));
    TEST_ASSERT_TRUE(device->is_allocated);

    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &event);
    TEST_ASSERT_FALSE(device->is_allocated);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xusbh_static_slots_start_free_and_reject_invalid_args);
    RUN_TEST(test_xusbh_slot_exhaustion_is_explicit);
    RUN_TEST(test_xusbh_release_cascades_interface_and_endpoint_slots);
    RUN_TEST(test_xusbh_transfer_submit_cancel_and_release_use_hcd_ops);
    RUN_TEST(test_xusbh_transfer_submit_propagates_hcd_failure);
    RUN_TEST(test_xusbh_transfer_completion_event_releases_submission_ownership);
    RUN_TEST(test_xusbh_port_disconnect_cleanup_releases_owned_slots);
    RUN_TEST(test_xusbh_disconnect_event_runs_port_cleanup);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
