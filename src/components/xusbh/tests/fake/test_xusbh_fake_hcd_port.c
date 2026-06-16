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

// @file test_xusbh_fake_hcd_port.c
// @brief Host tests for the fake xUSBH HCD port.

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "unity.h"
#include "xusbh_drv.h"

// MACROS //////////////////////////////////////////////////////////////////////

// TYPES ///////////////////////////////////////////////////////////////////////

// VARIABLES ///////////////////////////////////////////////////////////////////
static xUSBH_Fake_HCD_Context_t g_fake_hcd;
static uint32_t g_callback_count;
static xUSBH_HCD_Event_t g_last_event;

// EXTERN VARIABLES ////////////////////////////////////////////////////////////

// FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
static void hcd_callback(void *host_ctx, const xUSBH_HCD_Event_t *event);

// INLINE FUNCTIONS ////////////////////////////////////////////////////////////

// MODULE FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////
static void hcd_callback(void *host_ctx, const xUSBH_HCD_Event_t *event)
{
    TEST_ASSERT_NOT_NULL(host_ctx);
    TEST_ASSERT_NOT_NULL(event);

    g_callback_count++;
    g_last_event = *event;
}

// PUBLIC FUNCTIONS IMPLEMENTATION ////////////////////////////////////////////
void setUp(void)
{
    g_callback_count = 0U;
    (void)xUSBH_Fake_HCD_Init(&g_fake_hcd);
}

void tearDown(void)
{
}

void test_fake_hcd_exposes_complete_ops_and_defaults(void)
{
    TEST_ASSERT_TRUE(xUSBH_HCD_Ops_Are_Complete(&xUSBH_Fake_HCD_Ops));
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, g_fake_hcd.port_status.speed);
    TEST_ASSERT_EQUAL_UINT32(xUSBH_FAKE_HCD_DEFAULT_FRAME_NUMBER, g_fake_hcd.frame_number);
}

void test_fake_hcd_queues_and_completes_transfer(void)
{
    uint32_t host_ctx = 0x12345678U;
    uint8_t buffer[8] = {0};
    xUSBH_Transfer_t transfer = {
        .endpoint_address = 0x81U,
        .data = buffer,
        .length = sizeof(buffer),
    };

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Init(&xUSBH_Fake_HCD_Ops, &g_fake_hcd, &host_ctx, hcd_callback));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Start(&xUSBH_Fake_HCD_Ops, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Enable_Interrupts(&xUSBH_Fake_HCD_Ops, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Submit_Transfer(&xUSBH_Fake_HCD_Ops, &g_fake_hcd, &transfer));

    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.transfer_count);
    TEST_ASSERT_EQUAL_PTR(&transfer, g_fake_hcd.last_transfer);

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Fake_HCD_Complete_Transfer(&g_fake_hcd, &transfer, xUSBH_HCD_TRANSFER_EVENT_COMPLETE, 4U));

    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.transfer_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_callback_count);
    TEST_ASSERT_EQUAL(xUSBH_HCD_EVENT_TYPE_TRANSFER, g_last_event.type);
    TEST_ASSERT_EQUAL(xUSBH_HCD_TRANSFER_EVENT_COMPLETE, g_last_event.transfer_event);
    TEST_ASSERT_EQUAL_PTR(&transfer, g_last_event.transfer);
    TEST_ASSERT_EQUAL_UINT32(4U, transfer.actual_length);
}

void test_fake_hcd_submitted_pop_captures_transfer_without_callback(void)
{
    uint32_t host_ctx = 0x87654321U;
    xUSBH_Transfer_t transfer = {
        .endpoint_address = 1U,
    };
    xUSBH_Transfer_t *popped_transfer = NULL;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Init(&xUSBH_Fake_HCD_Ops, &g_fake_hcd, &host_ctx, hcd_callback));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Submit_Transfer(&xUSBH_Fake_HCD_Ops, &g_fake_hcd, &transfer));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Fake_HCD_Submitted_Pop(&g_fake_hcd, &popped_transfer));

    TEST_ASSERT_EQUAL_PTR(&transfer, popped_transfer);
    TEST_ASSERT_EQUAL_UINT32(0U, g_fake_hcd.transfer_count);
    TEST_ASSERT_EQUAL_UINT32(0U, g_callback_count);
}

void test_fake_hcd_port_event_uses_registered_callback(void)
{
    uint32_t host_ctx = 0x11111111U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Init(&xUSBH_Fake_HCD_Ops, &g_fake_hcd, &host_ctx, hcd_callback));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Enable_Interrupts(&xUSBH_Fake_HCD_Ops, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Fake_HCD_Fire_Port_Event(&g_fake_hcd, 0U, xUSBH_HCD_PORT_EVENT_CONNECTED));

    TEST_ASSERT_EQUAL_UINT32(1U, g_callback_count);
    TEST_ASSERT_EQUAL(xUSBH_HCD_EVENT_TYPE_PORT, g_last_event.type);
    TEST_ASSERT_EQUAL_UINT8(0U, g_last_event.port);
    TEST_ASSERT_EQUAL(xUSBH_HCD_PORT_EVENT_CONNECTED, g_last_event.port_event);
}

// MAIN ////////////////////////////////////////////////////////////////////////
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_fake_hcd_exposes_complete_ops_and_defaults);
    RUN_TEST(test_fake_hcd_queues_and_completes_transfer);
    RUN_TEST(test_fake_hcd_submitted_pop_captures_transfer_without_callback);
    RUN_TEST(test_fake_hcd_port_event_uses_registered_callback);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
