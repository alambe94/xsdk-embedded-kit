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

// @file test_dfu_protocol.c
// @brief Host tests for the xUSBD DFU state machine:
//        DNLOAD -> GETSTATUS -> DNLOAD_IDLE -> manifest and error-recovery paths.
//        Control requests are injected via SETUP_RECEIVED / DATA_RECEIVED / DATA_SENT
//        events, matching the full EP0 control-transfer state machine.

#include <string.h>

#include "test_helpers.h"
#include "xusbd_dfu.h"

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

// DFU interface index (first interface allocated after test_device_init).
#define DFU_IFACE 0U

// Test fixtures ///////////////////////////////////////////////////////////////

static xUSBD_Device_Context_t g_device;
static xUSBD_DFU_Context_t g_dfu;

static xUSBD_DFU_Callbacks_t g_dfu_calls = {
    .on_bus_event = fake_dfu_bus_event,
    .on_io_control = fake_dfu_io_control,
};

// EP0 control-transfer helpers ////////////////////////////////////////////////

static void fire_setup(USB_Setup_Request_t *req)
{
    dcd_fire_event(&g_device, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)req, (uint32_t)sizeof(*req));
}

// Device's ZLP IN (status for OUT transfers) was sent -> drives CTF_Complete.
static void fire_ep0_data_sent(void)
{
    dcd_fire_event(&g_device, USB_DCD_DATA_SENT, 0x80U, NULL, 0U);
}

// Host's ZLP OUT ack (status for IN transfers) -> drives CTF_Complete.
static void fire_ep0_host_zlp(void)
{
    dcd_fire_event(&g_device, USB_DCD_DATA_RECEIVED, 0x00U, NULL, 0U);
}

// Fire a class IN request (GETSTATUS, GETSTATE, ...) and complete the transfer.
// After SETUP the device sends data; then the host ACKs with a ZLP which
// triggers dfu_control_transfer_complete.
static void dfu_fire_in_req(uint8_t b_request, uint16_t w_length)
{
    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = b_request;
    req.wIndex = xCPU_TO_LE16(DFU_IFACE);
    req.wLength = xCPU_TO_LE16(w_length);
    fire_setup(&req);
    fire_ep0_host_zlp(); // host ZLP ack -> CTF_Complete
}

// Fire a class OUT request with no data (CLRSTATUS, ABORT, ...) and complete.
// The device sends a ZLP IN status; DATA_SENT drives CTF_Complete.
static void dfu_fire_out_no_data(uint8_t b_request, uint16_t w_value)
{
    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = b_request;
    req.wValue = xCPU_TO_LE16(w_value);
    req.wIndex = xCPU_TO_LE16(DFU_IFACE);
    req.wLength = xCPU_TO_LE16(0U);
    fire_setup(&req);
    fire_ep0_data_sent(); // ZLP status -> CTF_Complete
}

// Fire DNLOAD with block data and complete the transfer.
// EP0 OUT receive is set up on SETUP; DATA_RECEIVED delivers the block;
// device sends ZLP; DATA_SENT drives CTF_Complete.
static void dfu_fire_dnload_block(uint16_t block_num, uint16_t block_length)
{
    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = USB_DFU_REQ_DNLOAD;
    req.wValue = xCPU_TO_LE16(block_num);
    req.wIndex = xCPU_TO_LE16(DFU_IFACE);
    req.wLength = xCPU_TO_LE16(block_length);
    fire_setup(&req);
    // Fake DCD never DMA-copies into control_data, so pre-fill with non-zero bytes.
    memset(g_device.control_data, 0xA5U, block_length);
    dcd_fire_event(&g_device, USB_DCD_DATA_RECEIVED, 0x00U, g_device.control_data, block_length);
    fire_ep0_data_sent(); // ZLP status -> CTF_Complete
}

// Fire DNLOAD with wLength=0 (end-of-image, triggers manifest).
static void dfu_fire_dnload_end(void)
{
    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = USB_DFU_REQ_DNLOAD;
    req.wIndex = xCPU_TO_LE16(DFU_IFACE);
    req.wLength = xCPU_TO_LE16(0U);
    fire_setup(&req);
    fire_ep0_data_sent(); // ZLP status -> CTF_Complete
}

// Short-hands
#define dfu_getstatus() dfu_fire_in_req(USB_DFU_REQ_GETSTATUS, (uint16_t)sizeof(USB_DFU_Status_Response_t))
#define dfu_getstate()  dfu_fire_in_req(USB_DFU_REQ_GETSTATE, 1U)
#define dfu_clrstatus() dfu_fire_out_no_data(USB_DFU_REQ_CLRSTATUS, 0U)
#define dfu_abort()     dfu_fire_out_no_data(USB_DFU_REQ_ABORT, 0U)

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

// Shared registration helper used by multiple tests.
static void mode_start(void)
{
    (void)xUSBD_Class_Register(&g_device, &g_dfu.class_ctx, xUSBD_DFU_Mode_Class());
    (void)xUSBD_DFU_Set_Callbacks(&g_dfu.class_ctx, &g_dfu_calls);
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);
}

static void runtime_start(void)
{
    (void)xUSBD_Class_Register(&g_device, &g_dfu.class_ctx, xUSBD_DFU_Runtime_Class());
    (void)xUSBD_DFU_Set_Callbacks(&g_dfu.class_ctx, &g_dfu_calls);
    (void)test_device_start(&g_device);
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);
}

// MODE - GETSTATE / GETSTATUS /////////////////////////////////////////////////

void test_dfu_mode_getstate_returns_dfu_idle(void)
{
    mode_start();

    RESET_FAKE(fake_dcd_ep_send);
    dfu_getstate();

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(0x80U, fake_dcd_ep_send_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.arg3_val);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)xUSBD_DFU_STATE_DFU_IDLE, g_device.control_data[0]);
}

void test_dfu_mode_getstatus_returns_6_bytes(void)
{
    mode_start();

    RESET_FAKE(fake_dcd_ep_send);
    dfu_getstatus();

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(USB_DFU_Status_Response_t), fake_dcd_ep_send_fake.arg3_val);
}

// MODE - DNLOAD -> DNLOAD_SYNC / PENDING_WRITE /////////////////////////////////

void test_dfu_mode_dnload_block_transitions_to_dnload_sync(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U);
    // State is set synchronously during the OUT data handler
    TEST_ASSERT_EQUAL(xUSBD_DFU_STATE_DFU_DNLOAD_SYNC, g_dfu.state);
}

void test_dfu_mode_dnload_block_sets_pending_write_after_complete(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U);
    // CTF_Complete (fired by DATA_SENT) sets pending_op
    TEST_ASSERT_EQUAL(xUSBD_DFU_PENDING_WRITE, xUSBD_DFU_Get_Pending_Op(&g_dfu.class_ctx));
}

void test_dfu_mode_dnload_stores_block_data(void)
{
    mode_start();
    dfu_fire_dnload_block(3U, 64U);
    TEST_ASSERT_EQUAL_UINT16(3U, g_dfu.block_num);
    TEST_ASSERT_EQUAL_UINT32(64U, g_dfu.dnload_length);
    // Verify first byte was copied from control_data (0xA5 was pre-filled)
    TEST_ASSERT_EQUAL_UINT8(0xA5U, g_dfu.dnload_buffer[0]);
}

// MODE - GETSTATUS while pending (DNBUSY) /////////////////////////////////////

void test_dfu_mode_getstatus_during_write_reports_dnbusy(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U); // -> DNLOAD_SYNC, pending=WRITE

    RESET_FAKE(fake_dcd_ep_send);
    // Fire GETSTATUS without CTF_Complete host ZLP yet to check the wire state
    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = USB_DFU_REQ_GETSTATUS;
    req.wIndex = xCPU_TO_LE16(DFU_IFACE);
    req.wLength = xCPU_TO_LE16((uint16_t)sizeof(USB_DFU_Status_Response_t));
    fire_setup(&req);

    // Check response built in control_data: bState should be virtual DNBUSY
    USB_DFU_Status_Response_t *resp = (USB_DFU_Status_Response_t *)g_device.control_data;
    TEST_ASSERT_EQUAL_UINT8((uint8_t)xUSBD_DFU_STATE_DFU_DNBUSY, resp->bState);
}

// MODE - Process pending write ////////////////////////////////////////////////

void test_dfu_mode_process_write_calls_io_control(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U); // -> pending=WRITE

    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dfu_io_control_fake.call_count);
    TEST_ASSERT_EQUAL(xUSBD_DFU_IO_CMD_WRITE_BLOCK, fake_dfu_io_control_fake.arg1_val);
}

void test_dfu_mode_process_write_clears_pending_op(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U);
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);
    TEST_ASSERT_EQUAL(xUSBD_DFU_PENDING_NONE, xUSBD_DFU_Get_Pending_Op(&g_dfu.class_ctx));
}

// MODE - GETSTATUS after write done -> DNLOAD_IDLE /////////////////////////////

void test_dfu_mode_getstatus_after_write_transitions_to_dnload_idle(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U);                      // -> DNLOAD_SYNC, pending=WRITE
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx); // -> pending=NONE
    // Now GETSTATUS CTF_Complete should advance to DNLOAD_IDLE
    dfu_getstatus();
    TEST_ASSERT_EQUAL(xUSBD_DFU_STATE_DFU_DNLOAD_IDLE, g_dfu.state);
}

// MODE - DNLOAD(0) -> MANIFEST_SYNC ///////////////////////////////////////////

void test_dfu_mode_dnload_end_transitions_to_manifest_sync(void)
{
    mode_start();
    // Drive to DNLOAD_IDLE first
    dfu_fire_dnload_block(0U, 128U);
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);
    dfu_getstatus(); // -> DNLOAD_IDLE

    dfu_fire_dnload_end(); // DNLOAD(0)
    TEST_ASSERT_EQUAL(xUSBD_DFU_STATE_DFU_MANIFEST_SYNC, g_dfu.state);
}

void test_dfu_mode_dnload_end_sets_pending_manifest(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U);
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);
    dfu_getstatus();

    dfu_fire_dnload_end();
    TEST_ASSERT_EQUAL(xUSBD_DFU_PENDING_MANIFEST, xUSBD_DFU_Get_Pending_Op(&g_dfu.class_ctx));
}

// MODE - GETSTATUS while manifest pending (MANIFEST) //////////////////////////

void test_dfu_mode_getstatus_during_manifest_reports_manifest(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U);
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);
    dfu_getstatus();
    dfu_fire_dnload_end(); // -> MANIFEST_SYNC, pending=MANIFEST

    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = USB_DFU_REQ_GETSTATUS;
    req.wIndex = xCPU_TO_LE16(DFU_IFACE);
    req.wLength = xCPU_TO_LE16((uint16_t)sizeof(USB_DFU_Status_Response_t));
    fire_setup(&req);

    USB_DFU_Status_Response_t *resp = (USB_DFU_Status_Response_t *)g_device.control_data;
    TEST_ASSERT_EQUAL_UINT8((uint8_t)xUSBD_DFU_STATE_DFU_MANIFEST, resp->bState);
}

// MODE - Process manifest / MANIFEST_WAIT_RESET ///////////////////////////////

void test_dfu_mode_process_manifest_calls_io_control(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U);
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);
    dfu_getstatus();
    dfu_fire_dnload_end(); // -> pending=MANIFEST

    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);

    TEST_ASSERT_EQUAL(xUSBD_DFU_IO_CMD_MANIFEST, fake_dfu_io_control_fake.arg1_val);
}

void test_dfu_mode_getstatus_after_manifest_transitions_to_wait_reset(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U);
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);
    dfu_getstatus();
    dfu_fire_dnload_end();
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx); // -> pending=NONE

    dfu_getstatus(); // MANIFEST_SYNC + pending=NONE -> MANIFEST_WAIT_RESET
    TEST_ASSERT_EQUAL(xUSBD_DFU_STATE_DFU_MANIFEST_WAIT_RESET, g_dfu.state);
}

// MODE - RESET in MANIFEST_WAIT_RESET /////////////////////////////////////////

void test_dfu_mode_reset_in_manifest_wait_sets_pending_detach(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U);
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);
    dfu_getstatus();
    dfu_fire_dnload_end();
    (void)xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);
    dfu_getstatus(); // -> MANIFEST_WAIT_RESET

    dcd_fire_event(&g_device, USB_DCD_RESET_RECEIVED, 0U, NULL, 0U);

    TEST_ASSERT_EQUAL(xUSBD_DFU_PENDING_DETACH, xUSBD_DFU_Get_Pending_Op(&g_dfu.class_ctx));
}

// MODE - ABORT ////////////////////////////////////////////////////////////////

void test_dfu_mode_abort_from_dnload_sync_returns_to_idle(void)
{
    mode_start();
    dfu_fire_dnload_block(0U, 128U); // -> DNLOAD_SYNC

    dfu_abort();
    TEST_ASSERT_EQUAL(xUSBD_DFU_STATE_DFU_IDLE, g_dfu.state);
    TEST_ASSERT_EQUAL(xUSBD_DFU_PENDING_NONE, xUSBD_DFU_Get_Pending_Op(&g_dfu.class_ctx));
}

// MODE - CLRSTATUS from DFU_ERROR /////////////////////////////////////////////

void test_dfu_mode_clrstatus_from_error_returns_to_idle(void)
{
    mode_start();
    // Manually inject error state (as Process_Pending_Op would if write fails)
    g_dfu.state = xUSBD_DFU_STATE_DFU_ERROR;
    g_dfu.dfu_status = xUSBD_DFU_STATUS_ERR_WRITE;

    dfu_clrstatus();

    TEST_ASSERT_EQUAL(xUSBD_DFU_STATE_DFU_IDLE, g_dfu.state);
    TEST_ASSERT_EQUAL(xUSBD_DFU_STATUS_OK, g_dfu.dfu_status);
    TEST_ASSERT_EQUAL(xUSBD_DFU_PENDING_NONE, xUSBD_DFU_Get_Pending_Op(&g_dfu.class_ctx));
}

// MODE - Process with no pending op ///////////////////////////////////////////

void test_dfu_mode_process_no_pending_is_noop(void)
{
    mode_start();
    // pending_op starts NONE after init
    xRETURN_t status = xUSBD_DFU_Process_Pending_Op(&g_dfu.class_ctx);
    TEST_ASSERT_EQUAL(xRETURN_OK, status);
    TEST_ASSERT_EQUAL_UINT32(0U, fake_dfu_io_control_fake.call_count);
}

// RUNTIME - DETACH ////////////////////////////////////////////////////////////

void test_dfu_runtime_detach_transitions_to_app_detach(void)
{
    runtime_start();
    // DFU_DETACH is a no-data OUT request
    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = USB_DFU_REQ_DETACH;
    req.wIndex = xCPU_TO_LE16(DFU_IFACE);
    req.wLength = xCPU_TO_LE16(0U);
    fire_setup(&req);

    // State changes synchronously in control_out_request
    TEST_ASSERT_EQUAL(xUSBD_DFU_STATE_APP_DETACH, g_dfu.state);
}

void test_dfu_runtime_detach_sets_pending_detach_after_ctf_complete(void)
{
    runtime_start();
    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = USB_DFU_REQ_DETACH;
    req.wIndex = xCPU_TO_LE16(DFU_IFACE);
    req.wLength = xCPU_TO_LE16(0U);
    fire_setup(&req);
    fire_ep0_data_sent(); // ZLP status -> CTF_Complete -> pending_op = DETACH

    TEST_ASSERT_EQUAL(xUSBD_DFU_PENDING_DETACH, xUSBD_DFU_Get_Pending_Op(&g_dfu.class_ctx));
}

void test_dfu_runtime_getstatus_returns_6_bytes(void)
{
    runtime_start();

    RESET_FAKE(fake_dcd_ep_send);
    dfu_getstatus();

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(USB_DFU_Status_Response_t), fake_dcd_ep_send_fake.arg3_val);
}

void test_dfu_runtime_getstate_returns_app_idle(void)
{
    runtime_start();

    RESET_FAKE(fake_dcd_ep_send);
    dfu_getstate();

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.arg3_val);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)xUSBD_DFU_STATE_APP_IDLE, g_device.control_data[0]);
}

// RUNNER //////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_dfu_mode_getstate_returns_dfu_idle);
    RUN_TEST(test_dfu_mode_getstatus_returns_6_bytes);

    RUN_TEST(test_dfu_mode_dnload_block_transitions_to_dnload_sync);
    RUN_TEST(test_dfu_mode_dnload_block_sets_pending_write_after_complete);
    RUN_TEST(test_dfu_mode_dnload_stores_block_data);

    RUN_TEST(test_dfu_mode_getstatus_during_write_reports_dnbusy);

    RUN_TEST(test_dfu_mode_process_write_calls_io_control);
    RUN_TEST(test_dfu_mode_process_write_clears_pending_op);

    RUN_TEST(test_dfu_mode_getstatus_after_write_transitions_to_dnload_idle);

    RUN_TEST(test_dfu_mode_dnload_end_transitions_to_manifest_sync);
    RUN_TEST(test_dfu_mode_dnload_end_sets_pending_manifest);

    RUN_TEST(test_dfu_mode_getstatus_during_manifest_reports_manifest);

    RUN_TEST(test_dfu_mode_process_manifest_calls_io_control);
    RUN_TEST(test_dfu_mode_getstatus_after_manifest_transitions_to_wait_reset);

    RUN_TEST(test_dfu_mode_reset_in_manifest_wait_sets_pending_detach);

    RUN_TEST(test_dfu_mode_abort_from_dnload_sync_returns_to_idle);
    RUN_TEST(test_dfu_mode_clrstatus_from_error_returns_to_idle);
    RUN_TEST(test_dfu_mode_process_no_pending_is_noop);

    RUN_TEST(test_dfu_runtime_detach_transitions_to_app_detach);
    RUN_TEST(test_dfu_runtime_detach_sets_pending_detach_after_ctf_complete);
    RUN_TEST(test_dfu_runtime_getstatus_returns_6_bytes);
    RUN_TEST(test_dfu_runtime_getstate_returns_app_idle);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
