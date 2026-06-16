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

// @file test_xusbh_class.c
// @brief Host tests for xUSBH class registration and interface binding.

#include <string.h>

#include "unity.h"

#include "xusbh_class.h"
#include "test_xusbh_helpers.h"

typedef struct Class_Test_Context_t
{
    uint32_t start_count;
    uint32_t stop_count;
    uint32_t transfer_complete_count;
} Class_Test_Context_t;

static xUSBH_Context_t g_host;
static Class_Test_Context_t g_hid_class;
static Class_Test_Context_t g_storage_class;
static Class_Test_Context_t g_any_class;

static xRETURN_t hid_match(const xUSBH_Interface_Context_t *interface_ctx, bool *is_match)
{
    *is_match = interface_ctx->class_code == USB_CLASS_HID;

    return xRETURN_OK;
}

static xRETURN_t storage_match(const xUSBH_Interface_Context_t *interface_ctx, bool *is_match)
{
    *is_match = interface_ctx->class_code == USB_CLASS_STORAGE;

    return xRETURN_OK;
}

static xRETURN_t any_match(const xUSBH_Interface_Context_t *interface_ctx, bool *is_match)
{
    (void)interface_ctx;

    *is_match = true;

    return xRETURN_OK;
}

static xRETURN_t class_start(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx)
{
    Class_Test_Context_t *ctx = (Class_Test_Context_t *)class_ctx;

    (void)interface_ctx;
    ctx->start_count++;

    return xRETURN_OK;
}

static xRETURN_t class_stop(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx)
{
    Class_Test_Context_t *ctx = (Class_Test_Context_t *)class_ctx;

    (void)interface_ctx;
    ctx->stop_count++;

    return xRETURN_OK;
}

static xRETURN_t class_transfer_complete(xUSBH_Interface_Context_t *interface_ctx, void *class_ctx, const xUSBH_Transfer_t *transfer)
{
    Class_Test_Context_t *ctx = (Class_Test_Context_t *)class_ctx;

    (void)interface_ctx;
    (void)transfer;
    ctx->transfer_complete_count++;

    return xRETURN_OK;
}

static const xUSBH_Class_Driver_t hid_driver = {
    .match = hid_match,
    .start = class_start,
    .stop = class_stop,
    .transfer_complete = class_transfer_complete,
};

static const xUSBH_Class_Driver_t storage_driver = {
    .match = storage_match,
    .start = class_start,
    .stop = class_stop,
    .transfer_complete = class_transfer_complete,
};

static const xUSBH_Class_Driver_t any_driver = {
    .match = any_match,
    .start = class_start,
    .stop = class_stop,
    .transfer_complete = class_transfer_complete,
};

static void host_init_only(void)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&g_host, &valid_init_config));
}

static void host_init_and_start(void)
{
    host_init_only();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&g_host, &valid_start_config));
}

static void register_driver(const xUSBH_Class_Driver_t *driver, void *class_ctx)
{
    xUSBH_Class_Register_Config_t config = {
        .driver = driver,
        .class_ctx = class_ctx,
    };

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Register_Class(&g_host, &config));
}

static void allocate_device_interface_endpoint(xUSBH_Device_Context_t **device,
                                               xUSBH_Interface_Context_t **interface,
                                               xUSBH_Endpoint_Context_t **endpoint)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Allocate(&g_host, 0U, device));
    (*device)->address = 2U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Interface_Allocate(&g_host, *device, interface));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, *interface, endpoint));
    (*endpoint)->endpoint_address = 0x81U;
    (*endpoint)->endpoint_type = USB_ENDP_TYPE_INTR;
    (*endpoint)->is_in = true;
    (*endpoint)->max_packet_size = 8U;
}

void setUp(void)
{
    (void)memset(&g_host, 0, sizeof(g_host));
    reset_fake_hcd();
    (void)memset(&g_hid_class, 0, sizeof(g_hid_class));
    (void)memset(&g_storage_class, 0, sizeof(g_storage_class));
    (void)memset(&g_any_class, 0, sizeof(g_any_class));
}

void tearDown(void)
{
}

void test_xusbh_class_registration_requires_initialized_not_started_host(void)
{
    xUSBH_Class_Register_Config_t config = {
        .driver = &hid_driver,
        .class_ctx = &g_hid_class,
    };

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_Register_Class(NULL, &config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NOT_INITIALIZED, xUSBH_Register_Class(&g_host, &config));

    host_init_only();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Register_Class(&g_host, &config));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_STATE, xUSBH_Register_Class(&g_host, &config));

    host_init_and_start();
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_STATE, xUSBH_Register_Class(&g_host, &config));
}

void test_xusbh_class_bind_starts_matching_interface_and_ignores_unsupported(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;

    host_init_only();
    register_driver(&hid_driver, &g_hid_class);
    allocate_device_interface_endpoint(&device, &interface, &endpoint);
    interface->class_code = USB_CLASS_HID;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_EQUAL_UINT32(1U, g_hid_class.start_count);
    TEST_ASSERT_EQUAL_PTR(&hid_driver, interface->class_driver);
    TEST_ASSERT_EQUAL_PTR(&g_hid_class, interface->class_ctx);

    interface->class_driver = NULL;
    interface->class_ctx = NULL;
    interface->class_code = USB_CLASS_STORAGE;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_EQUAL_UINT32(1U, g_hid_class.start_count);
    TEST_ASSERT_NULL(interface->class_driver);
}

void test_xusbh_class_bind_rejects_ambiguous_matches(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;

    host_init_only();
    register_driver(&hid_driver, &g_hid_class);
    register_driver(&any_driver, &g_any_class);
    allocate_device_interface_endpoint(&device, &interface, &endpoint);
    interface->class_code = USB_CLASS_HID;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_AMBIGUOUS_CLASS_MATCH, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_EQUAL_UINT32(0U, g_hid_class.start_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_any_class.start_count);
    TEST_ASSERT_NULL(interface->class_driver);
}

void test_xusbh_class_transfer_complete_routes_to_endpoint_owner_only(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *hid_interface = NULL;
    xUSBH_Interface_Context_t *storage_interface = NULL;
    xUSBH_Endpoint_Context_t *hid_endpoint = NULL;
    xUSBH_Endpoint_Context_t *storage_endpoint = NULL;
    xUSBH_Transfer_t transfer = {
        .device_address = 2U,
        .endpoint_address = 0x81U,
    };

    host_init_only();
    register_driver(&hid_driver, &g_hid_class);
    register_driver(&storage_driver, &g_storage_class);
    allocate_device_interface_endpoint(&device, &hid_interface, &hid_endpoint);
    hid_interface->class_code = USB_CLASS_HID;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Interface_Allocate(&g_host, device, &storage_interface));
    storage_interface->class_code = USB_CLASS_STORAGE;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, storage_interface, &storage_endpoint));
    storage_endpoint->endpoint_address = 0x02U;
    storage_endpoint->endpoint_type = USB_ENDP_TYPE_BULK;
    storage_endpoint->max_packet_size = 64U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Transfer_Complete(&g_host, &transfer));
    TEST_ASSERT_EQUAL_UINT32(1U, g_hid_class.transfer_complete_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_storage_class.transfer_complete_count);
}

void test_xusbh_device_release_notifies_bound_class_before_slot_cleanup(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *endpoint = NULL;

    host_init_only();
    register_driver(&hid_driver, &g_hid_class);
    allocate_device_interface_endpoint(&device, &interface, &endpoint);
    interface->class_code = USB_CLASS_HID;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Release(&g_host, device));

    TEST_ASSERT_EQUAL_UINT32(1U, g_hid_class.stop_count);
    TEST_ASSERT_FALSE(interface->is_allocated);
    TEST_ASSERT_FALSE(endpoint->is_allocated);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xusbh_class_registration_requires_initialized_not_started_host);
    RUN_TEST(test_xusbh_class_bind_starts_matching_interface_and_ignores_unsupported);
    RUN_TEST(test_xusbh_class_bind_rejects_ambiguous_matches);
    RUN_TEST(test_xusbh_class_transfer_complete_routes_to_endpoint_owner_only);
    RUN_TEST(test_xusbh_device_release_notifies_bound_class_before_slot_cleanup);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
