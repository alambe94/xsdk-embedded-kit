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

// @file test_xusbh_hcd.c
// @brief Host tests for xUSBH HCD wrapper validation and dispatch.

#include <string.h>

#include "unity.h"

#include "xusbh_hcd.h"
#include "test_xusbh_helpers.h"

static uint8_t g_host_marker;

static void fake_event_callback(void *host_ctx, const xUSBH_HCD_Event_t *event)
{
    (void)host_ctx;
    (void)event;
}

void setUp(void)
{
    reset_fake_hcd();
    g_fake_hcd.frame_number = 1234U; // HCD wrapper tests assert this exact value
}

void tearDown(void)
{
}

void test_xusbh_hcd_ops_complete_detects_missing_callbacks(void)
{
    xUSBH_HCD_Ops_t incomplete_ops = fake_hcd_ops;

    TEST_ASSERT_FALSE(xUSBH_HCD_Ops_Are_Complete(NULL));
    TEST_ASSERT_TRUE(xUSBH_HCD_Ops_Are_Complete(&fake_hcd_ops));

    incomplete_ops.get_frame_number = NULL;
    TEST_ASSERT_FALSE(xUSBH_HCD_Ops_Are_Complete(&incomplete_ops));
}

void test_xusbh_hcd_wrappers_reject_null_args(void)
{
    xUSBH_Transfer_t transfer = {0};

    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Init(NULL, &g_fake_hcd, &g_host_marker, fake_event_callback));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Init(&fake_hcd_ops, &g_fake_hcd, NULL, fake_event_callback));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Init(&fake_hcd_ops, &g_fake_hcd, &g_host_marker, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Start(NULL, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Stop(NULL, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Deinit(NULL, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Enable_Interrupts(NULL, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Disable_Interrupts(NULL, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Port_Power(NULL, &g_fake_hcd, 0U, true));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_HCD_NULL_POINTER, xUSBH_HCD_Port_Reset(NULL, &g_fake_hcd, 0U));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_HCD_Get_Port_Status(&fake_hcd_ops, &g_fake_hcd, 0U, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_HCD_Submit_Transfer(&fake_hcd_ops, &g_fake_hcd, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_HCD_Cancel_Transfer(&fake_hcd_ops, &g_fake_hcd, NULL));
    TEST_ASSERT_EQUAL(xRETURN_xERR_xUSBH_NULL_POINTER, xUSBH_HCD_Get_Frame_Number(&fake_hcd_ops, &g_fake_hcd, NULL));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Submit_Transfer(&fake_hcd_ops, &g_fake_hcd, &transfer));
}

void test_xusbh_hcd_wrappers_dispatch_to_ops(void)
{
    xUSBH_HCD_Port_Status_t status = {0};
    xUSBH_Transfer_t transfer = {
        .device_address = 2U,
        .endpoint_address = 0x81U,
        .endpoint_type = USB_ENDP_TYPE_INTR,
    };
    uint32_t frame_number = 0U;

    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Init(&fake_hcd_ops, &g_fake_hcd, &g_host_marker, fake_event_callback));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Start(&fake_hcd_ops, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Enable_Interrupts(&fake_hcd_ops, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Port_Power(&fake_hcd_ops, &g_fake_hcd, 0U, true));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Port_Reset(&fake_hcd_ops, &g_fake_hcd, 0U));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Get_Port_Status(&fake_hcd_ops, &g_fake_hcd, 0U, &status));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Submit_Transfer(&fake_hcd_ops, &g_fake_hcd, &transfer));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Cancel_Transfer(&fake_hcd_ops, &g_fake_hcd, &transfer));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Get_Frame_Number(&fake_hcd_ops, &g_fake_hcd, &frame_number));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Disable_Interrupts(&fake_hcd_ops, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Stop(&fake_hcd_ops, &g_fake_hcd));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_HCD_Deinit(&fake_hcd_ops, &g_fake_hcd));

    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.init_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.start_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.enable_interrupts_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.port_power_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.port_reset_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.get_port_status_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.submit_transfer_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.cancel_transfer_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.get_frame_number_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.disable_interrupts_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.stop_count);
    TEST_ASSERT_EQUAL_UINT32(1U, g_fake_hcd.deinit_count);
    TEST_ASSERT_EQUAL_PTR(&g_host_marker, g_fake_hcd.last_host_ctx);
    TEST_ASSERT_EQUAL_PTR(fake_event_callback, g_fake_hcd.last_callback);
    TEST_ASSERT_EQUAL_PTR(&transfer, g_fake_hcd.last_transfer);
    TEST_ASSERT_TRUE(g_fake_hcd.last_port_power_enable);
    TEST_ASSERT_TRUE(status.is_connected);
    TEST_ASSERT_TRUE(status.is_enabled);
    TEST_ASSERT_EQUAL(USB_SPEED_HIGH, status.speed);
    TEST_ASSERT_EQUAL_UINT32(1234U, frame_number);
}

// MAIN ////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_xusbh_hcd_ops_complete_detects_missing_callbacks);
    RUN_TEST(test_xusbh_hcd_wrappers_reject_null_args);
    RUN_TEST(test_xusbh_hcd_wrappers_dispatch_to_ops);
    return UNITY_END();
}
// EOF /////////////////////////////////////////////////////////////////////////////
