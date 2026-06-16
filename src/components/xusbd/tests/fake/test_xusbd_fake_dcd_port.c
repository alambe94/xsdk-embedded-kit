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

// @file test_xusbd_fake_dcd_port.c
// @brief Host tests for the fake xUSBD DCD port.

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "unity.h"
#include "xusbd_drv.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////
static xUSBD_Fake_DCD_Context_t g_fake_dcd;
static uint32_t g_callback_count;
static USB_DCD_Event_t g_last_event;
static uint8_t g_last_ep_addr;
static uint8_t *g_last_data;
static uint32_t g_last_length;
static uint32_t g_transfer_complete_count;
static xRETURN_t g_transfer_status;
static uint32_t g_transfer_actual_length;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
static void dcd_callback(void *device_ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length);
static void transfer_complete(void *user_ctx, const xUSBD_DCD_Transfer_t *transfer, xRETURN_t status, uint32_t actual_length);

// INLINE FUNCTIONS ////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////
static void dcd_callback(void *device_ctx, USB_DCD_Event_t event, uint8_t ep_addr, uint8_t *data, uint32_t length)
{
    TEST_ASSERT_NOT_NULL(device_ctx);

    g_callback_count++;
    g_last_event = event;
    g_last_ep_addr = ep_addr;
    g_last_data = data;
    g_last_length = length;
}

static void transfer_complete(void *user_ctx, const xUSBD_DCD_Transfer_t *transfer, xRETURN_t status, uint32_t actual_length)
{
    TEST_ASSERT_NOT_NULL(user_ctx);
    TEST_ASSERT_NOT_NULL(transfer);

    g_transfer_complete_count++;
    g_transfer_status = status;
    g_transfer_actual_length = actual_length;
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////
void setUp(void)
{
    g_callback_count = 0U;
    g_last_event = USB_DCD_SETUP_RECEIVED;
    g_last_ep_addr = 0U;
    g_last_data = NULL;
    g_last_length = 0U;
    g_transfer_complete_count = 0U;
    g_transfer_status = xRETURN_OK;
    g_transfer_actual_length = 0U;
    (void)xUSBD_Fake_DCD_Init(&g_fake_dcd);
}

void tearDown(void)
{
}

void test_fake_dcd_defaults_and_scalar_ops(void)
{
    uint32_t frame_number = 99U;
    USB_Speed_t speed = USB_SPEED_FULL;

    TEST_ASSERT_NOT_NULL(xUSBD_Fake_DCD_Ops.init);
    TEST_ASSERT_NOT_NULL(xUSBD_Fake_DCD_Ops.ep_transfer_queue);
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, g_fake_dcd.speed);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DCD_Get_Frame_Number(&xUSBD_Fake_DCD_Ops, &g_fake_dcd, &frame_number));
    TEST_ASSERT_EQUAL_UINT32(xUSBD_FAKE_DCD_DEFAULT_FRAME_NUMBER, frame_number);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DCD_Get_Speed(&xUSBD_Fake_DCD_Ops, &g_fake_dcd, &speed));
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, speed);
}

void test_fake_dcd_receive_injects_armed_out_data_event(void)
{
    uint32_t device_ctx = 0x1234U;
    uint8_t rx_buffer[8] = {0};
    const uint8_t host_data[4] = {0x10U, 0x20U, 0x30U, 0x40U};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DCD_Init(&xUSBD_Fake_DCD_Ops, &g_fake_dcd, USB_SPEED_HIGH, &device_ctx));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DCD_Set_Event_Callback(&xUSBD_Fake_DCD_Ops, &g_fake_dcd, dcd_callback));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DCD_Enable_Interrupts(&xUSBD_Fake_DCD_Ops, &g_fake_dcd));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DCD_EP_Receive(&xUSBD_Fake_DCD_Ops, &g_fake_dcd, 0x01U, rx_buffer, sizeof(rx_buffer)));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Fake_DCD_RX(&g_fake_dcd, 0x01U, host_data, sizeof(host_data)));

    TEST_ASSERT_EQUAL_UINT32(1U, g_callback_count);
    TEST_ASSERT_EQUAL(USB_DCD_DATA_RECEIVED, g_last_event);
    TEST_ASSERT_EQUAL_UINT8(0x01U, g_last_ep_addr);
    TEST_ASSERT_EQUAL_PTR(rx_buffer, g_last_data);
    TEST_ASSERT_EQUAL_UINT32(sizeof(host_data), g_last_length);
    TEST_ASSERT_EQUAL_MEMORY(host_data, rx_buffer, sizeof(host_data));
}

void test_fake_dcd_send_is_captured_by_tx_pop(void)
{
    uint8_t ep_addr = 0U;
    uint8_t popped[8] = {0};
    uint32_t length = 0U;
    bool is_zlp_required = false;
    uint8_t payload[3] = {0xA0U, 0xB0U, 0xC0U};

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DCD_EP_Send(&xUSBD_Fake_DCD_Ops, &g_fake_dcd, 0x81U, payload, sizeof(payload), true));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Fake_DCD_TX_Pop(&g_fake_dcd, &ep_addr, popped, sizeof(popped), &length, &is_zlp_required));

    TEST_ASSERT_EQUAL_UINT8(0x81U, ep_addr);
    TEST_ASSERT_EQUAL_UINT32(sizeof(payload), length);
    TEST_ASSERT_TRUE(is_zlp_required);
    TEST_ASSERT_EQUAL_MEMORY(payload, popped, sizeof(payload));
}

void test_fake_dcd_transfer_queue_can_complete_callback(void)
{
    uint32_t user_ctx = 0xABCDU;
    uint8_t payload[4] = {0};
    xUSBD_DCD_Transfer_t transfer = {
        .ep_addr = 0x82U,
        .data = payload,
        .length = sizeof(payload),
        .complete = transfer_complete,
        .user_ctx = &user_ctx,
    };

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_DCD_EP_Transfer_Queue(&xUSBD_Fake_DCD_Ops, &g_fake_dcd, &transfer));
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_dcd.transfer_count);
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBD_Fake_DCD_Complete_Transfer(&g_fake_dcd, xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT, 2U));

    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_dcd.transfer_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_transfer_complete_count);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBD_DCD_INVALID_ENDPOINT, g_transfer_status);
    TEST_ASSERT_EQUAL_UINT32(2U, g_transfer_actual_length);
}

// MAIN ////////////////////////////////////////////////////////////////////////
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fake_dcd_defaults_and_scalar_ops);
    RUN_TEST(test_fake_dcd_receive_injects_armed_out_data_event);
    RUN_TEST(test_fake_dcd_send_is_captured_by_tx_pop);
    RUN_TEST(test_fake_dcd_transfer_queue_can_complete_callback);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
