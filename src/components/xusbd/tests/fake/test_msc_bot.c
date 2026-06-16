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

// @file test_msc_bot.c
// @brief Host tests for the xUSBD MSC BOT (Bulk-Only Transport) protocol layer.
//        Exercises the CBW->SCSI->CSW pipeline by injecting DATA_RECEIVED and
//        DATA_SENT events directly into the registered DCD callback.

#include <string.h>

#include "test_helpers.h"
#include "xusbd_msc.h"

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

// Test-wide disk geometry used by GET_CAPACITY and RW10 tests.
#define TEST_BLOCKS     512U
#define TEST_BLOCK_SIZE 512U

// MSC endpoint addresses assigned by xUSBD_Init (next_in_ep=0x81, next_out_ep=0x01).
#define MSC_IN_EP  0x81U
#define MSC_OUT_EP 0x01U

// BOT direction flags
#define CBW_DIR_IN  0x80U
#define CBW_DIR_OUT 0x00U

// Test fixtures ///////////////////////////////////////////////////////////////

static xUSBD_Device_Context_t g_device;
static xUSBD_MSC_Context_t g_msc;
static xUSBD_MSC_Capacity_t g_capacity = {TEST_BLOCKS, TEST_BLOCK_SIZE};
static uint32_t g_lun_count = 1U;
static uint8_t g_read_buf[TEST_BLOCK_SIZE];

static xUSBD_MSC_Callbacks_t g_msc_calls = {
    .on_bus_event = fake_msc_bus_event,
    .on_io_control = fake_msc_io_control,
};

// io_control custom fake - handles the commands that return output pointers.
static xRETURN_t io_ctrl_impl(xUSBD_Class_Context_t *class_ctx,
                              xUSBD_MSC_IO_CMD_t cmd,
                              void *cmd_buff,
                              uint32_t cmd_length,
                              void **data_buff,
                              uint32_t *data_length)
{
    (void)class_ctx;
    (void)cmd_buff;
    (void)cmd_length;
    switch (cmd)
    {
    case xUSBD_MSC_IO_CMD_GET_CAPACITY:
        if (data_buff != NULL)
        {
            *data_buff = &g_capacity;
        }
        if (data_length != NULL)
        {
            *data_length = (uint32_t)sizeof(g_capacity);
        }
        return xRETURN_OK;
    case xUSBD_MSC_IO_CMD_GET_LUN:
        if (data_buff != NULL)
        {
            *data_buff = &g_lun_count;
        }
        if (data_length != NULL)
        {
            *data_length = (uint32_t)sizeof(g_lun_count);
        }
        return xRETURN_OK;
    case xUSBD_MSC_IO_CMD_INQUIRY:
        if (data_buff != NULL)
        {
            *data_buff = g_read_buf;
        }
        if (data_length != NULL)
        {
            *data_length = 36U;
        }
        return xRETURN_OK;
    case xUSBD_MSC_IO_CMD_GET_READ_ADDR: /* fall through */
    case xUSBD_MSC_IO_CMD_GET_WRITE_ADDR:
        if (data_buff != NULL)
        {
            *data_buff = g_read_buf;
        }
        if (data_length != NULL)
        {
            *data_length = TEST_BLOCK_SIZE;
        }
        return xRETURN_OK;
    default:
        return xRETURN_OK;
    }
}

// Helpers /////////////////////////////////////////////////////////////////////

// Fire a USB setup packet into the DCD event callback.
static void fire_setup(USB_Setup_Request_t *req)
{
    dcd_fire_event(&g_device, USB_DCD_SETUP_RECEIVED, 0x00U, (uint8_t *)req, (uint32_t)sizeof(*req));
}

// Fire a DATA_SENT (IN complete) on EP0 - advances the EP0 control state machine.
static void fire_ep0_data_sent(void)
{
    dcd_fire_event(&g_device, USB_DCD_DATA_SENT, 0x80U, NULL, 0U);
}

// Drive the full USB enumeration sequence so that MSC receives CONFIGURED_RECEIVED.
// After this call the MSC bulk endpoints are initialised and the driver waits for a CBW.
static void msc_do_configure(void)
{
    (void)xUSBD_MSC_Set_Callbacks(&g_msc.class_ctx, &g_msc_calls);
    (void)test_device_start(&g_device);

    // USB CONNECT -> EP0 initialised, CONNECT forwarded to class drivers
    dcd_fire_event(&g_device, USB_DCD_CONNECT_RECEIVED, 0U, NULL, 0U);

    // SET_ADDRESS(1) - sets is_addressed after status ZLP
    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE);
    req.bRequest = USB_REQ_SET_ADDRESS;
    req.wValue = xCPU_TO_LE16(1U);
    fire_setup(&req);
    fire_ep0_data_sent();

    // SET_CONFIGURATION(1) - fires CONFIGURED_RECEIVED to class drivers on status ZLP
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_OUT | USB_REQ_TYPE_STANDARD | USB_REQ_RECIPIENT_DEVICE);
    req.bRequest = USB_REQ_SET_CONFIGURATION;
    req.wValue = xCPU_TO_LE16(1U);
    fire_setup(&req);
    fire_ep0_data_sent();
}

// Build a minimal valid CBW.
static void make_cbw(USB_MSC_BOT_CBW_t *cbw, uint32_t data_length, uint8_t flags, uint8_t opcode)
{
    memset(cbw, 0, sizeof(*cbw));
    uint32_t sig = USB_MSC_BOT_CBW_SIGNATURE;
    memcpy(cbw->dCBWSignature, &sig, 4U);
    uint32_t tag = 0x00000001U;
    memcpy(cbw->dCBWTag, &tag, 4U);
    memcpy(cbw->dCBWDataLength, &data_length, 4U);
    cbw->bmCBWFlags = flags;
    cbw->bCBWCBLength = 1U;
    cbw->CBWCB[0] = opcode;
}

// Simulate the host sending a CBW on the bulk-OUT endpoint.
// The MSC driver reads ctx->cbw directly (ignores the data pointer passed to the callback)
// because the real DCD already DMA-copied the data into the registered receive buffer.
// Copy into the driver's internal buffer so msc_cbw_validate reads the correct bytes.
static void fire_cbw(USB_MSC_BOT_CBW_t *cbw)
{
    memcpy(&g_msc.cbw, cbw, sizeof(g_msc.cbw));
    dcd_fire_event(&g_device, USB_DCD_DATA_RECEIVED, MSC_OUT_EP, (uint8_t *)cbw, USB_MSC_BOT_CBW_LENGTH);
}

// Simulate the DCD notifying that the device finished sending data on the bulk-IN endpoint.
static void fire_in_data_sent(void)
{
    dcd_fire_event(&g_device, USB_DCD_DATA_SENT, MSC_IN_EP, NULL, 0U);
}

void setUp(void)
{
    RESET_ALL_DCD_FAKES();
    RESET_MSC_FAKES();
    memset(&g_device, 0, sizeof(g_device));
    memset(&g_msc, 0, sizeof(g_msc));
    memset(&g_read_buf, 0, sizeof(g_read_buf));
    test_device_init(&g_device);
    (void)xUSBD_Class_Register(&g_device, &g_msc.class_ctx, xUSBD_MSC_Class());

    fake_msc_io_control_fake.custom_fake = io_ctrl_impl;
    fake_msc_bus_event_fake.return_val = xRETURN_OK;
}

void tearDown(void)
{
}

// CONFIGURE FLOW //////////////////////////////////////////////////////////////

void test_msc_bot_configure_inits_four_endpoints(void)
{
    // 2 for EP0 (from CONNECT) + 2 for MSC bulk (from CONFIGURED_RECEIVED)
    msc_do_configure();
    TEST_ASSERT_EQUAL_UINT32(4U, fake_dcd_ep_init_fake.call_count);
}

void test_msc_bot_configure_starts_cbw_receive(void)
{
    // ep_receive called for EP0 (from CONNECT) + once for MSC CBW (from CONFIGURED)
    msc_do_configure();
    TEST_ASSERT_EQUAL_UINT32(2U, fake_dcd_ep_receive_fake.call_count);
    // Last receive must be on the MSC OUT endpoint
    TEST_ASSERT_EQUAL_UINT8(MSC_OUT_EP, fake_dcd_ep_receive_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(USB_MSC_BOT_CBW_LENGTH, fake_dcd_ep_receive_fake.arg3_val);
}

// CBW VALIDATION //////////////////////////////////////////////////////////////

void test_msc_bot_invalid_cbw_signature_stalls_both_endpoints(void)
{
    msc_do_configure();

    USB_MSC_BOT_CBW_t cbw;
    make_cbw(&cbw, 0U, CBW_DIR_OUT, USB_MSC_TEST_UNIT_READY);
    cbw.dCBWSignature[0] = 0xFFU; // corrupt signature

    RESET_MSC_FAKES();
    RESET_FAKE(fake_dcd_ep_stall);
    fire_cbw(&cbw);

    TEST_ASSERT_EQUAL_UINT32(2U, fake_dcd_ep_stall_fake.call_count);
}

void test_msc_bot_invalid_cbw_length_stalls_both_endpoints(void)
{
    msc_do_configure();

    USB_MSC_BOT_CBW_t cbw;
    make_cbw(&cbw, 0U, CBW_DIR_OUT, USB_MSC_TEST_UNIT_READY);

    RESET_MSC_FAKES();
    RESET_FAKE(fake_dcd_ep_stall);
    // Fire with length != USB_MSC_BOT_CBW_LENGTH - treated as invalid CBW
    dcd_fire_event(&g_device, USB_DCD_DATA_RECEIVED, MSC_OUT_EP, (uint8_t *)&cbw, USB_MSC_BOT_CBW_LENGTH - 1U);

    TEST_ASSERT_EQUAL_UINT32(2U, fake_dcd_ep_stall_fake.call_count);
}

// NO-DATA SCSI COMMANDS ///////////////////////////////////////////////////////

void test_msc_bot_test_unit_ready_sends_passed_csw(void)
{
    msc_do_configure();

    USB_MSC_BOT_CBW_t cbw;
    make_cbw(&cbw, 0U, CBW_DIR_OUT, USB_MSC_TEST_UNIT_READY);

    RESET_FAKE(fake_dcd_ep_send);
    fire_cbw(&cbw);

    // CSW must be sent on the IN endpoint
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(MSC_IN_EP, fake_dcd_ep_send_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(USB_MSC_BOT_CSW_LENGTH, fake_dcd_ep_send_fake.arg3_val);

    // CSW status byte must be COMMAND_PASSED (0x00)
    uint8_t *csw_bytes = (uint8_t *)fake_dcd_ep_send_fake.arg2_val;
    TEST_ASSERT_NOT_NULL(csw_bytes);
    TEST_ASSERT_EQUAL_UINT8(0x00U, csw_bytes[USB_MSC_BOT_CSW_LENGTH - 1U]);
}

void test_msc_bot_start_stop_unit_sends_passed_csw(void)
{
    msc_do_configure();

    USB_MSC_BOT_CBW_t cbw;
    make_cbw(&cbw, 0U, CBW_DIR_OUT, USB_MSC_START_STOP_UNIT);

    RESET_FAKE(fake_dcd_ep_send);
    fire_cbw(&cbw);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    uint8_t *csw_bytes = (uint8_t *)fake_dcd_ep_send_fake.arg2_val;
    TEST_ASSERT_NOT_NULL(csw_bytes);
    TEST_ASSERT_EQUAL_UINT8(0x00U, csw_bytes[USB_MSC_BOT_CSW_LENGTH - 1U]);
}

// IN-DATA SCSI COMMANDS ///////////////////////////////////////////////////////

void test_msc_bot_request_sense_sends_18_bytes(void)
{
    msc_do_configure();

    USB_MSC_BOT_CBW_t cbw;
    make_cbw(&cbw, 18U, CBW_DIR_IN, USB_MSC_REQUEST_SENSE);

    RESET_FAKE(fake_dcd_ep_send);
    fire_cbw(&cbw);

    // First ep_send must be the 18-byte REQUEST SENSE response
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(MSC_IN_EP, fake_dcd_ep_send_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(18U, fake_dcd_ep_send_fake.arg3_val);
}

void test_msc_bot_read_capacity_sends_8_bytes(void)
{
    msc_do_configure();

    USB_MSC_BOT_CBW_t cbw;
    make_cbw(&cbw, 8U, CBW_DIR_IN, USB_MSC_READ_CAPACITY);

    RESET_FAKE(fake_dcd_ep_send);
    fire_cbw(&cbw);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(MSC_IN_EP, fake_dcd_ep_send_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(8U, fake_dcd_ep_send_fake.arg3_val);

    // Verify big-endian block count and block size in the response
    uint8_t *resp = (uint8_t *)fake_dcd_ep_send_fake.arg2_val;
    TEST_ASSERT_NOT_NULL(resp);
    uint32_t last_lba = ((uint32_t)resp[0] << 24) | ((uint32_t)resp[1] << 16) | ((uint32_t)resp[2] << 8) | (uint32_t)resp[3];
    uint32_t blk_size = ((uint32_t)resp[4] << 24) | ((uint32_t)resp[5] << 16) | ((uint32_t)resp[6] << 8) | (uint32_t)resp[7];
    TEST_ASSERT_EQUAL_UINT32(TEST_BLOCKS - 1U, last_lba);
    TEST_ASSERT_EQUAL_UINT32(TEST_BLOCK_SIZE, blk_size);
}

void test_msc_bot_inquiry_sends_data(void)
{
    msc_do_configure();

    USB_MSC_BOT_CBW_t cbw;
    make_cbw(&cbw, 36U, CBW_DIR_IN, USB_MSC_INQUIRY);

    RESET_FAKE(fake_dcd_ep_send);
    fire_cbw(&cbw);

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(MSC_IN_EP, fake_dcd_ep_send_fake.arg1_val);
}

// CSW + RESTART FLOW //////////////////////////////////////////////////////////

void test_msc_bot_data_in_sent_triggers_csw(void)
{
    msc_do_configure();

    USB_MSC_BOT_CBW_t cbw;
    make_cbw(&cbw, 8U, CBW_DIR_IN, USB_MSC_READ_CAPACITY);
    fire_cbw(&cbw); // sends DATA IN (8 bytes)

    RESET_FAKE(fake_dcd_ep_send);
    fire_in_data_sent(); // DATA IN complete -> MSC sends CSW

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(MSC_IN_EP, fake_dcd_ep_send_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(USB_MSC_BOT_CSW_LENGTH, fake_dcd_ep_send_fake.arg3_val);
}

void test_msc_bot_csw_sent_restarts_cbw_receive(void)
{
    msc_do_configure();

    USB_MSC_BOT_CBW_t cbw;
    make_cbw(&cbw, 8U, CBW_DIR_IN, USB_MSC_READ_CAPACITY);
    fire_cbw(&cbw);      // DATA IN
    fire_in_data_sent(); // CSW

    RESET_FAKE(fake_dcd_ep_receive);
    fire_in_data_sent(); // CSW complete -> MSC queues next CBW receive

    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_receive_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(MSC_OUT_EP, fake_dcd_ep_receive_fake.arg1_val);
    TEST_ASSERT_EQUAL_UINT32(USB_MSC_BOT_CBW_LENGTH, fake_dcd_ep_receive_fake.arg3_val);
}

// CONTROL REQUESTS ////////////////////////////////////////////////////////////

void test_msc_get_max_lun_returns_lun(void)
{
    msc_do_configure();

    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = USB_MSC_REQ_GET_MAX_LUN;
    req.wIndex = xCPU_TO_LE16(0U);
    req.wLength = xCPU_TO_LE16(1U);

    RESET_FAKE(fake_dcd_ep_send);
    fire_setup(&req);

    // Stack should have sent 1 byte (max LUN index = lun_count - 1)
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_send_fake.call_count);
    uint8_t *data = (uint8_t *)fake_dcd_ep_send_fake.arg2_val;
    TEST_ASSERT_NOT_NULL(data);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(g_lun_count - 1U), data[0]);
}

void test_msc_bomsr_resets_and_clears_stalls(void)
{
    msc_do_configure();

    USB_Setup_Request_t req;
    memset(&req, 0, sizeof(req));
    req.bRequestType = (uint8_t)(USB_REQ_TYPE_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIPIENT_INTERFACE);
    req.bRequest = USB_MSC_REQ_SET_BOMSR;
    req.wIndex = xCPU_TO_LE16(0U);
    req.wLength = xCPU_TO_LE16(0U);

    RESET_FAKE(fake_dcd_ep_clear_stall);
    RESET_FAKE(fake_dcd_ep_receive);
    fire_setup(&req);

    // BOMSR must clear stall on both bulk endpoints
    TEST_ASSERT_EQUAL_UINT32(2U, fake_dcd_ep_clear_stall_fake.call_count);
    // And restart CBW receive
    TEST_ASSERT_EQUAL_UINT32(1U, fake_dcd_ep_receive_fake.call_count);
    TEST_ASSERT_EQUAL_UINT8(MSC_OUT_EP, fake_dcd_ep_receive_fake.arg1_val);
}

// RUNNER //////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_msc_bot_configure_inits_four_endpoints);
    RUN_TEST(test_msc_bot_configure_starts_cbw_receive);

    RUN_TEST(test_msc_bot_invalid_cbw_signature_stalls_both_endpoints);
    RUN_TEST(test_msc_bot_invalid_cbw_length_stalls_both_endpoints);

    RUN_TEST(test_msc_bot_test_unit_ready_sends_passed_csw);
    RUN_TEST(test_msc_bot_start_stop_unit_sends_passed_csw);

    RUN_TEST(test_msc_bot_request_sense_sends_18_bytes);
    RUN_TEST(test_msc_bot_read_capacity_sends_8_bytes);
    RUN_TEST(test_msc_bot_inquiry_sends_data);

    RUN_TEST(test_msc_bot_data_in_sent_triggers_csw);
    RUN_TEST(test_msc_bot_csw_sent_restarts_cbw_receive);

    RUN_TEST(test_msc_get_max_lun_returns_lun);
    RUN_TEST(test_msc_bomsr_resets_and_clears_stalls);

    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
