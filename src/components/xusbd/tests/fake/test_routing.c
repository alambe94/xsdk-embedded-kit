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

// @file test_routing.c
// @brief Host tests for class dispatch: control requests routed by interface
// and endpoint owner, and data events routed to the correct class.

#include "unity.h"
#include "test_helpers.h"

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
}
void tearDown(void)
{
}

// ROUTING PROBE DRIVER ////////////////////////////////////////////////////////

typedef struct
{
    uint8_t interface;
    uint8_t in_ep;
    uint8_t out_ep;
    uint32_t control_in_calls;
    uint32_t control_out_calls;
    uint32_t data_received_calls;
    uint32_t data_sent_calls;
    uint32_t control_complete_calls;
    uint8_t last_ep;
    xRETURN_t control_in_status;
    xRETURN_t control_out_status;
} Routing_Probe_t;

static Routing_Probe_t *probe_from(xUSBD_Class_Context_t *class_ctx)
{
    void *ctx = NULL;
    return (xUSBD_Class_Get_App_Context(class_ctx, &ctx) == xRETURN_OK) ? ctx : NULL;
}

static xRETURN_t routing_init_instance(xUSBD_Class_Context_t *class_ctx)
{
    Routing_Probe_t *probe = probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    xRETURN_t status = xUSBD_Class_Allocate_Interface(class_ctx, &probe->interface);
    if (status != xRETURN_OK)
    {
        return status;
    }

    status = xUSBD_Class_Allocate_Endpoint(class_ctx, 0x00U, &probe->out_ep);
    if (status != xRETURN_OK)
    {
        return status;
    }

    return xUSBD_Class_Allocate_Endpoint(class_ctx, 0x80U, &probe->in_ep);
}

static uint32_t routing_build_descriptor(xUSBD_Class_Context_t *class_ctx, uint8_t *buffer, USB_Speed_t speed)
{
    Routing_Probe_t *probe = probe_from(class_ctx);
    if (probe == NULL)
    {
        return 0U;
    }

    uint8_t *ptr = build_interface_descriptor(buffer, probe->interface, 0U, 2U, USB_CLASS_VENDOR, 0U, 0U, 0U);
    ptr = build_endpoint_descriptor(ptr, probe->out_ep, USB_ENDP_TYPE_BULK, 64U, 0U, speed, 0U, 0U, 0U);
    ptr = build_endpoint_descriptor(ptr, probe->in_ep, USB_ENDP_TYPE_BULK, 64U, 0U, speed, 0U, 0U, 0U);
    return (uint32_t)(ptr - buffer);
}

static xRETURN_t routing_control_in(xUSBD_Class_Context_t *class_ctx, xUSBD_Response_t *response)
{
    static uint8_t response_data = 0xA5U;

    Routing_Probe_t *probe = probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    probe->control_in_calls++;
    response->data = &response_data;
    response->length = sizeof(response_data);
    if (probe->control_in_status != xRETURN_OK)
    {
        return probe->control_in_status;
    }
    return xRETURN_OK;
}

static xRETURN_t routing_control_out(xUSBD_Class_Context_t *class_ctx, uint8_t *data, uint32_t length)
{
    (void)data;
    (void)length;

    Routing_Probe_t *probe = probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    probe->control_out_calls++;
    if (probe->control_out_status != xRETURN_OK)
    {
        return probe->control_out_status;
    }
    return xRETURN_OK;
}

static xRETURN_t routing_data_received(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)data;
    (void)length;

    Routing_Probe_t *probe = probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    probe->data_received_calls++;
    probe->last_ep = ep_addr;
    return xRETURN_OK;
}

static xRETURN_t routing_data_sent(xUSBD_Class_Context_t *class_ctx, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    (void)data;
    (void)length;

    Routing_Probe_t *probe = probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    probe->data_sent_calls++;
    probe->last_ep = ep_addr;
    return xRETURN_OK;
}

static xRETURN_t routing_control_complete(xUSBD_Class_Context_t *class_ctx, USB_Setup_Request_t *request)
{
    (void)request;

    Routing_Probe_t *probe = probe_from(class_ctx);
    if (probe == NULL)
    {
        return xRETURN_xERR_xUSBD_NULL_POINTER;
    }

    probe->control_complete_calls++;
    return xRETURN_OK;
}

static xUSBD_Class_Driver_t routing_driver = {
    .init_instance = routing_init_instance,
    .build_descriptor = routing_build_descriptor,
    .control_in_request = routing_control_in,
    .control_out_request = routing_control_out,
    .data_received = routing_data_received,
    .transmit_complete = routing_data_sent,
    .control_transfer_complete = routing_control_complete,
};

// TESTS ///////////////////////////////////////////////////////////////////////

void test_interface_control_request_routes_to_owner(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_a = {0};
    xUSBD_Class_Context_t class_b = {0};
    Routing_Probe_t probe_a = {0};
    Routing_Probe_t probe_b = {0};
    USB_Setup_Request_t request = {0};
    xUSBD_Response_t response = {0};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_App_Context(&class_a, &probe_a));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_App_Context(&class_b, &probe_b));
    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_a, &routing_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_b, &routing_driver, NULL));

    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    request.wIndex = xCPU_TO_LE16(probe_b.interface);
    device_ctx.request = request;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_In_Request_Process(&device_ctx, &response));
    TEST_ASSERT_EQUAL_UINT32(0U, probe_a.control_in_calls);
    TEST_ASSERT_EQUAL_UINT32(1U, probe_b.control_in_calls);
    TEST_ASSERT_EQUAL_PTR(&class_b, device_ctx.control_request_class);
    TEST_ASSERT_NOT_NULL(response.data);
    TEST_ASSERT_EQUAL_UINT32(1U, response.length);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Control_Transfer_Complete_Process(&device_ctx, &request));
    TEST_ASSERT_EQUAL_UINT32(0U, probe_a.control_complete_calls);
    TEST_ASSERT_EQUAL_UINT32(1U, probe_b.control_complete_calls);
    TEST_ASSERT_NULL(device_ctx.control_request_class);
}

void test_endpoint_control_request_routes_to_owner(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_a = {0};
    xUSBD_Class_Context_t class_b = {0};
    Routing_Probe_t probe_a = {0};
    Routing_Probe_t probe_b = {0};
    USB_Setup_Request_t request = {0};
    uint8_t control_data[2] = {0U};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_App_Context(&class_a, &probe_a));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_App_Context(&class_b, &probe_b));
    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_a, &routing_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_b, &routing_driver, NULL));

    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_ENDPOINT;
    request.wIndex = xCPU_TO_LE16(probe_a.out_ep);
    device_ctx.request = request;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Out_Request_Process(&device_ctx, control_data, sizeof(control_data)));
    TEST_ASSERT_EQUAL_UINT32(1U, probe_a.control_out_calls);
    TEST_ASSERT_EQUAL_UINT32(0U, probe_b.control_out_calls);
    TEST_ASSERT_EQUAL_PTR(&class_a, device_ctx.control_request_class);
}

void test_endpoint_data_events_route_to_owner(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_a = {0};
    xUSBD_Class_Context_t class_b = {0};
    Routing_Probe_t probe_a = {0};
    Routing_Probe_t probe_b = {0};
    uint8_t data[2] = {0U};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_App_Context(&class_a, &probe_a));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_App_Context(&class_b, &probe_b));
    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_a, &routing_driver, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_b, &routing_driver, NULL));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Data_Received(&device_ctx, probe_b.out_ep, data, sizeof(data)));
    TEST_ASSERT_EQUAL_UINT32(0U, probe_a.data_received_calls);
    TEST_ASSERT_EQUAL_UINT32(1U, probe_b.data_received_calls);
    TEST_ASSERT_EQUAL_UINT8(probe_b.out_ep, probe_b.last_ep);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Data_Sent(&device_ctx, probe_a.in_ep, data, sizeof(data)));
    TEST_ASSERT_EQUAL_UINT32(1U, probe_a.data_sent_calls);
    TEST_ASSERT_EQUAL_UINT32(0U, probe_b.data_sent_calls);
    TEST_ASSERT_EQUAL_UINT8(probe_a.in_ep, probe_a.last_ep);
}

void test_control_request_propagates_driver_error(void)
{
    xUSBD_Device_Context_t device_ctx;
    xUSBD_Class_Context_t class_a = {0};
    Routing_Probe_t probe_a = {0};
    USB_Setup_Request_t request = {0};
    xUSBD_Response_t response = {0};
    uint8_t control_data[2] = {0U};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Class_Set_App_Context(&class_a, &probe_a));
    test_device_init(&device_ctx);

    TEST_ASSERT_EQUAL(xRETURN_OK, test_class_register(&device_ctx, &class_a, &routing_driver, NULL));

    // Test IN request error propagation
    probe_a.control_in_status = xRETURN_xERR_xUSBD_APP_NOT_INSTALLED;
    request.bRequestType = USB_REQ_TYPE_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE;
    request.wIndex = xCPU_TO_LE16(probe_a.interface);
    device_ctx.request = request;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_APP_NOT_INSTALLED, xUSBD_Class_In_Request_Process(&device_ctx, &response));

    // Test OUT request error propagation
    probe_a.control_out_status = xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE;
    request.bRequestType = USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_ENDPOINT;
    request.wIndex = xCPU_TO_LE16(probe_a.out_ep);
    device_ctx.request = request;
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_INSUFFICIENT_BUFFER_SIZE,
                      xUSBD_Class_Out_Request_Process(&device_ctx, control_data, sizeof(control_data)));
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_interface_control_request_routes_to_owner);
    RUN_TEST(test_endpoint_control_request_routes_to_owner);
    RUN_TEST(test_endpoint_data_events_route_to_owner);
    RUN_TEST(test_control_request_propagates_driver_error);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////
