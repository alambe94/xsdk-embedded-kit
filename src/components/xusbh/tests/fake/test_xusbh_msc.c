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

// @file test_xusbh_msc.c
// @brief Host tests for xUSBH Mass Storage Class binding and BOT scaffolding.

#include <string.h>

#include "unity.h"

#include "xassert.h"
#include "xusb_setup.h"
#include "xusbh_msc.h"
#include "test_xusbh_helpers.h"

xSTATIC_ASSERT(sizeof(xUSBH_MSC_BOT_CBW_t) == USB_MSC_BOT_CBW_LENGTH, "xUSBH MSC CBW test wire size changed");
xSTATIC_ASSERT(sizeof(xUSBH_MSC_BOT_CSW_t) == USB_MSC_BOT_CSW_LENGTH, "xUSBH MSC CSW test wire size changed");

static xUSBH_Context_t g_host;
static xUSBH_MSC_Context_t g_msc;

static void host_init_register_msc(void)
{
    xUSBH_Class_Register_Config_t class_config = {
        .driver = xUSBH_MSC_Class(),
        .class_ctx = &g_msc,
    };

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(&g_host, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Init(&g_msc, &g_host));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Register_Class(&g_host, &class_config));
}

static void host_init_register_msc_and_start(void)
{
    host_init_register_msc();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(&g_host, &valid_start_config));
}

static void allocate_msc_interface(xUSBH_Device_Context_t **device,
                                   xUSBH_Interface_Context_t **interface,
                                   xUSBH_Endpoint_Context_t **bulk_in,
                                   xUSBH_Endpoint_Context_t **bulk_out)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Allocate(&g_host, 0U, device));
    (*device)->address = 4U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Interface_Allocate(&g_host, *device, interface));
    (*interface)->class_code = USB_CLASS_STORAGE;
    (*interface)->subclass = USB_MSC_SCSI_TRANSPARENT_COMMAND_SET;
    (*interface)->protocol = USB_MSC_BBB;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, *interface, bulk_in));
    (*bulk_in)->endpoint_address = 0x81U;
    (*bulk_in)->endpoint_type = USB_ENDP_TYPE_BULK;
    (*bulk_in)->is_in = true;
    (*bulk_in)->max_packet_size = 64U;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, *interface, bulk_out));
    (*bulk_out)->endpoint_address = 0x01U;
    (*bulk_out)->endpoint_type = USB_ENDP_TYPE_BULK;
    (*bulk_out)->is_in = false;
    (*bulk_out)->max_packet_size = 64U;
    (*interface)->endpoint_count = 2U;
}

void setUp(void)
{
    (void)memset(&g_host, 0, sizeof(g_host));
    (void)memset(&g_msc, 0, sizeof(g_msc));
    reset_fake_hcd();
}

void tearDown(void)
{
}

void test_xusbh_msc_binds_scsi_bulk_only_interface(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;

    host_init_register_msc();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_EQUAL_PTR(xUSBH_MSC_Class(), interface->class_driver);
    TEST_ASSERT_TRUE(g_msc.instances[0].is_allocated);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_READY, g_msc.instances[0].state);
    TEST_ASSERT_EQUAL_PTR(interface, g_msc.instances[0].interface_ctx);
    TEST_ASSERT_EQUAL_PTR(bulk_in, g_msc.instances[0].bulk_in_endpoint);
    TEST_ASSERT_EQUAL_PTR(bulk_out, g_msc.instances[0].bulk_out_endpoint);
}

void test_xusbh_msc_requires_bulk_in_and_bulk_out_endpoints(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;

    host_init_register_msc();
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Device_Allocate(&g_host, 0U, &device));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Interface_Allocate(&g_host, device, &interface));
    interface->class_code = USB_CLASS_STORAGE;
    interface->subclass = USB_MSC_SCSI_TRANSPARENT_COMMAND_SET;
    interface->protocol = USB_MSC_BBB;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Endpoint_Allocate(&g_host, interface, &bulk_in));
    bulk_in->endpoint_address = 0x81U;
    bulk_in->endpoint_type = USB_ENDP_TYPE_BULK;
    bulk_in->is_in = true;

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_OBJECT, xUSBH_Class_Bind_Device(&g_host, device));
    TEST_ASSERT_FALSE(g_msc.instances[0].is_allocated);
}

static void complete_transfer(xUSBH_Transfer_t *transfer, xUSBH_HCD_Transfer_Event_t transfer_event, uint32_t actual_length)
{
    xUSBH_HCD_Event_t event = {
        .type = xUSBH_HCD_EVENT_TYPE_TRANSFER,
        .transfer_event = transfer_event,
        .transfer = transfer,
    };

    transfer->actual_length = actual_length;
    g_fake_hcd.last_callback(g_fake_hcd.last_host_ctx, &event);
}

static void prepare_passed_csw(xUSBH_MSC_Instance_t *instance)
{
    uint8_t *csw = (uint8_t *)&instance->csw;

    xWrite_LE32(&csw[0U], USB_MSC_BOT_CSW_SIGNATURE);
    xWrite_LE32(&csw[4U], instance->active_tag);
    xWrite_LE32(&csw[8U], 0U);
    csw[12U] = 0U;
}

void test_xusbh_msc_read_write_api_validates_arguments_and_binding(void)
{
    uint8_t buffer[512] = {0};

    host_init_register_msc();

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_MSC_Read_Blocks(NULL, 0U, 0U, 1U, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_MSC_Write_Blocks(&g_msc, 0U, 0U, 1U, NULL, sizeof(buffer)));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_OBJECT, xUSBH_MSC_Read_Blocks(&g_msc, 0U, 0U, 1U, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_OBJECT, xUSBH_MSC_Write_Blocks(&g_msc, 0U, 0U, 1U, buffer, sizeof(buffer)));
}

void test_xusbh_msc_read_blocks_submits_read10_bot_sequence(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;
    uint8_t buffer[xUSBH_MSC_BLOCK_SIZE] = {0};

    host_init_register_msc_and_start();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Read_Blocks(&g_msc, 0U, 0x12345678U, 1U, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT8(0x01U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL_UINT32(USB_MSC_BOT_CBW_LENGTH, g_fake_hcd.last_transfer->length);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_COMMAND, g_msc.instances[0].state);

    const uint8_t *cbw = (const uint8_t *)&g_msc.instances[0].cbw;
    TEST_ASSERT_EQUAL_UINT32(USB_MSC_BOT_CBW_SIGNATURE, xRead_LE32(&cbw[0U]));
    TEST_ASSERT_EQUAL_UINT32(xUSBH_MSC_BLOCK_SIZE, xRead_LE32(&cbw[8U]));
    TEST_ASSERT_EQUAL_UINT8(xUSBH_MSC_BOT_CBW_FLAG_IN, cbw[12U]);
    TEST_ASSERT_EQUAL_UINT8(USB_MSC_READ10, cbw[15U]);
    TEST_ASSERT_EQUAL_UINT32(0x12345678U, xRead_BE32(&cbw[17U]));
    TEST_ASSERT_EQUAL_UINT16(1U, xRead_BE16(&cbw[22U]));

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, USB_MSC_BOT_CBW_LENGTH);
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT8(0x81U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL_PTR(buffer, g_fake_hcd.last_transfer->data);
    TEST_ASSERT_EQUAL_UINT32(xUSBH_MSC_BLOCK_SIZE, g_fake_hcd.last_transfer->length);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_DATA_IN, g_msc.instances[0].state);

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, xUSBH_MSC_BLOCK_SIZE);
    TEST_ASSERT_EQUAL_UINT32(3U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT8(0x81U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL_UINT32(USB_MSC_BOT_CSW_LENGTH, g_fake_hcd.last_transfer->length);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_STATUS, g_msc.instances[0].state);

    prepare_passed_csw(&g_msc.instances[0]);
    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, USB_MSC_BOT_CSW_LENGTH);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_READY, g_msc.instances[0].state);
    TEST_ASSERT_EQUAL(xUSBH_MSC_ERROR_NONE, g_msc.instances[0].error);
    TEST_ASSERT_NULL(g_msc.instances[0].transfer);
    TEST_ASSERT_FALSE(g_host.transfers[0].is_allocated);
}

void test_xusbh_msc_write_blocks_submits_write10_data_out(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;
    uint8_t buffer[xUSBH_MSC_BLOCK_SIZE] = {0};

    host_init_register_msc_and_start();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Write_Blocks(&g_msc, 0U, 2U, 1U, buffer, sizeof(buffer)));
    const uint8_t *cbw = (const uint8_t *)&g_msc.instances[0].cbw;
    TEST_ASSERT_EQUAL_UINT8(xUSBH_MSC_BOT_CBW_FLAG_OUT, cbw[12U]);
    TEST_ASSERT_EQUAL_UINT8(USB_MSC_WRITE10, cbw[15U]);

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, USB_MSC_BOT_CBW_LENGTH);
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT8(0x01U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL_PTR(buffer, g_fake_hcd.last_transfer->data);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_DATA_OUT, g_msc.instances[0].state);

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, xUSBH_MSC_BLOCK_SIZE);
    TEST_ASSERT_EQUAL_UINT8(0x81U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_STATUS, g_msc.instances[0].state);
}

void test_xusbh_msc_read_blocks_rejects_small_buffer(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;
    uint8_t buffer[xUSBH_MSC_BLOCK_SIZE - 1U] = {0};

    host_init_register_msc_and_start();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_INVALID_ARGUMENT, xUSBH_MSC_Read_Blocks(&g_msc, 0U, 0U, 1U, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.submit_transfer_count);
}

void test_xusbh_msc_inquiry_submits_six_byte_data_in_command(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;
    uint8_t buffer[xUSBH_MSC_INQUIRY_LENGTH] = {0};

    host_init_register_msc_and_start();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Inquiry(&g_msc, 0U, buffer, sizeof(buffer)));

    const uint8_t *cbw = (const uint8_t *)&g_msc.instances[0].cbw;
    TEST_ASSERT_EQUAL_UINT32(xUSBH_MSC_INQUIRY_LENGTH, xRead_LE32(&cbw[8U]));
    TEST_ASSERT_EQUAL_UINT8(xUSBH_MSC_BOT_CBW_FLAG_IN, cbw[12U]);
    TEST_ASSERT_EQUAL_UINT8(6U, cbw[14U]);
    TEST_ASSERT_EQUAL_UINT8(USB_MSC_INQUIRY, cbw[15U]);
    TEST_ASSERT_EQUAL_UINT8(xUSBH_MSC_INQUIRY_LENGTH, cbw[19U]);

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, USB_MSC_BOT_CBW_LENGTH);
    TEST_ASSERT_EQUAL_UINT8(0x81U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL_PTR(buffer, g_fake_hcd.last_transfer->data);
    TEST_ASSERT_EQUAL_UINT32(xUSBH_MSC_INQUIRY_LENGTH, g_fake_hcd.last_transfer->length);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_DATA_IN, g_msc.instances[0].state);
}

void test_xusbh_msc_test_unit_ready_skips_data_stage(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;

    host_init_register_msc_and_start();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Test_Unit_Ready(&g_msc, 0U));

    const uint8_t *cbw = (const uint8_t *)&g_msc.instances[0].cbw;
    TEST_ASSERT_EQUAL_UINT32(0U, xRead_LE32(&cbw[8U]));
    TEST_ASSERT_EQUAL_UINT8(xUSBH_MSC_BOT_CBW_FLAG_OUT, cbw[12U]);
    TEST_ASSERT_EQUAL_UINT8(USB_MSC_TEST_UNIT_READY, cbw[15U]);

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, USB_MSC_BOT_CBW_LENGTH);
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT8(0x81U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL_UINT32(USB_MSC_BOT_CSW_LENGTH, g_fake_hcd.last_transfer->length);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_STATUS, g_msc.instances[0].state);
}

void test_xusbh_msc_read_capacity_parses_successful_response(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;
    xUSBH_MSC_Capacity_t capacity = {0};

    host_init_register_msc_and_start();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Read_Capacity(&g_msc, 0U, &capacity));
    const uint8_t *cbw = (const uint8_t *)&g_msc.instances[0].cbw;
    TEST_ASSERT_EQUAL_UINT32(8U, xRead_LE32(&cbw[8U]));
    TEST_ASSERT_EQUAL_UINT8(USB_MSC_READ_CAPACITY, cbw[15U]);

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, USB_MSC_BOT_CBW_LENGTH);
    TEST_ASSERT_EQUAL_UINT8(0x81U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL_PTR(g_msc.instances[0].capacity_buffer, g_fake_hcd.last_transfer->data);
    TEST_ASSERT_EQUAL_UINT32(8U, g_fake_hcd.last_transfer->length);

    xWrite_BE32(g_fake_hcd.last_transfer->data, 4095U);
    xWrite_BE32(&g_fake_hcd.last_transfer->data[4U], 512U);
    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, 8U);
    prepare_passed_csw(&g_msc.instances[0]);
    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, USB_MSC_BOT_CSW_LENGTH);

    TEST_ASSERT_EQUAL_UINT32(4096U, capacity.block_count);
    TEST_ASSERT_EQUAL_UINT32(512U, capacity.block_size);
    TEST_ASSERT_EQUAL_UINT32(512U, g_msc.instances[0].block_size);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_READY, g_msc.instances[0].state);
}

void test_xusbh_msc_request_sense_submits_fixed_data_in_command(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;
    uint8_t buffer[xUSBH_MSC_REQUEST_SENSE_LENGTH] = {0};

    host_init_register_msc_and_start();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Request_Sense(&g_msc, 0U, buffer, sizeof(buffer)));

    const uint8_t *cbw = (const uint8_t *)&g_msc.instances[0].cbw;
    TEST_ASSERT_EQUAL_UINT32(xUSBH_MSC_REQUEST_SENSE_LENGTH, xRead_LE32(&cbw[8U]));
    TEST_ASSERT_EQUAL_UINT8(xUSBH_MSC_BOT_CBW_FLAG_IN, cbw[12U]);
    TEST_ASSERT_EQUAL_UINT8(USB_MSC_REQUEST_SENSE, cbw[15U]);
    TEST_ASSERT_EQUAL_UINT8(xUSBH_MSC_REQUEST_SENSE_LENGTH, cbw[19U]);
}

void test_xusbh_msc_transfer_errors_are_recorded_explicitly(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;
    xUSBH_Transfer_t transfer = {
        .device_address = 4U,
        .endpoint_address = 0x81U,
        .length = 64U,
        .actual_length = 32U,
        .last_event = xUSBH_HCD_TRANSFER_EVENT_COMPLETE,
    };

    host_init_register_msc();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Transfer_Complete(&g_host, &transfer));
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_ERROR, g_msc.instances[0].state);
    TEST_ASSERT_EQUAL(xUSBH_MSC_ERROR_SHORT_TRANSFER, g_msc.instances[0].error);

    transfer.actual_length = 64U;
    transfer.last_event = xUSBH_HCD_TRANSFER_EVENT_STALLED;
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Transfer_Complete(&g_host, &transfer));
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_RESET_RECOVERY, g_msc.instances[0].state);
    TEST_ASSERT_EQUAL(xUSBH_MSC_ERROR_STALL, g_msc.instances[0].error);
}

void test_xusbh_msc_reset_recovery_submits_reset_and_clear_halt_sequence(void)
{
    xUSBH_Device_Context_t *device = NULL;
    xUSBH_Interface_Context_t *interface = NULL;
    xUSBH_Endpoint_Context_t *bulk_in = NULL;
    xUSBH_Endpoint_Context_t *bulk_out = NULL;
    uint8_t buffer[xUSBH_MSC_BLOCK_SIZE] = {0};

    host_init_register_msc_and_start();
    allocate_msc_interface(&device, &interface, &bulk_in, &bulk_out);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Class_Bind_Device(&g_host, device));
    interface->interface_number = 3U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Read_Blocks(&g_msc, 0U, 0U, 1U, buffer, sizeof(buffer)));
    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_STALLED, 0U);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_RESET_RECOVERY, g_msc.instances[0].state);
    TEST_ASSERT_NULL(g_msc.instances[0].transfer);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_MSC_Reset_Recovery(&g_msc, 0U));
    TEST_ASSERT_EQUAL_UINT32(2U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_TRUE(g_fake_hcd.last_transfer->has_setup);
    TEST_ASSERT_EQUAL_UINT8(0U, g_fake_hcd.last_transfer->endpoint_address);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE,
                            g_fake_hcd.last_transfer->setup.bRequestType);
    TEST_ASSERT_EQUAL_UINT8(USB_MSC_REQ_SET_BOMSR, g_fake_hcd.last_transfer->setup.bRequest);
    TEST_ASSERT_EQUAL_UINT16(3U, xUSB_Setup_Get_Index(&g_fake_hcd.last_transfer->setup));
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_RESET_REQUEST, g_msc.instances[0].state);

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, 0U);
    TEST_ASSERT_EQUAL_UINT32(3U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_CLEAR_FEATURE, g_fake_hcd.last_transfer->setup.bRequest);
    TEST_ASSERT_EQUAL_UINT16(0x81U, xUSB_Setup_Get_Index(&g_fake_hcd.last_transfer->setup));
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_CLEAR_IN_HALT, g_msc.instances[0].state);

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, 0U);
    TEST_ASSERT_EQUAL_UINT32(4U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT8(USB_REQ_CLEAR_FEATURE, g_fake_hcd.last_transfer->setup.bRequest);
    TEST_ASSERT_EQUAL_UINT16(0x01U, xUSB_Setup_Get_Index(&g_fake_hcd.last_transfer->setup));
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_CLEAR_OUT_HALT, g_msc.instances[0].state);

    complete_transfer(g_fake_hcd.last_transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, 0U);
    TEST_ASSERT_EQUAL(xUSBH_MSC_STATE_READY, g_msc.instances[0].state);
    TEST_ASSERT_EQUAL(xUSBH_MSC_ERROR_NONE, g_msc.instances[0].error);
    TEST_ASSERT_NULL(g_msc.instances[0].transfer);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xusbh_msc_binds_scsi_bulk_only_interface);
    RUN_TEST(test_xusbh_msc_requires_bulk_in_and_bulk_out_endpoints);
    RUN_TEST(test_xusbh_msc_read_write_api_validates_arguments_and_binding);
    RUN_TEST(test_xusbh_msc_read_blocks_submits_read10_bot_sequence);
    RUN_TEST(test_xusbh_msc_write_blocks_submits_write10_data_out);
    RUN_TEST(test_xusbh_msc_read_blocks_rejects_small_buffer);
    RUN_TEST(test_xusbh_msc_inquiry_submits_six_byte_data_in_command);
    RUN_TEST(test_xusbh_msc_test_unit_ready_skips_data_stage);
    RUN_TEST(test_xusbh_msc_read_capacity_parses_successful_response);
    RUN_TEST(test_xusbh_msc_request_sense_submits_fixed_data_in_command);
    RUN_TEST(test_xusbh_msc_transfer_errors_are_recorded_explicitly);
    RUN_TEST(test_xusbh_msc_reset_recovery_submits_reset_and_clear_halt_sequence);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
