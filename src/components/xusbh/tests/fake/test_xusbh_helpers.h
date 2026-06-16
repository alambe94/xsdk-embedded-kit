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

// @file test_xusbh_helpers.h
// @brief Shared fake HCD infrastructure for all xUSBH host tests.
//
// All globals and functions are `static` so each Unity test executable (one
// .c file each) gets its own isolated copy - no link-time conflicts.
//
// Usage:
//   1. #include "test_xusbh_helpers.h" at the top of the test file.
//   2. Call reset_fake_hcd() in setUp().
//   3. Optionally override specific return values or port_status fields before
//      each test that needs non-default HCD behavior.

#ifndef TEST_XUSBH_HELPERS_H
#define TEST_XUSBH_HELPERS_H

#include <stdbool.h>
#include <string.h>

#include "unity.h"
#include "xusbh_core.h"

// FAKE HCD CONTEXT /////////////////////////////////////////////////////////////
// Superset of all per-test Fake_HCD_Context_t variants used across the nine
// xUSBH test files.  Fields unused by a particular test default to zero /
// xRETURN_OK and are simply ignored.

typedef struct Fake_HCD_Context_t
{
    // Call counts
    uint32_t init_count;
    uint32_t deinit_count;
    uint32_t start_count;
    uint32_t stop_count;
    uint32_t enable_interrupts_count;
    uint32_t disable_interrupts_count;
    uint32_t port_power_count;
    uint32_t port_reset_count;
    uint32_t get_port_status_count;
    uint32_t submit_transfer_count;
    uint32_t cancel_transfer_count;
    uint32_t get_frame_number_count;

    // Captured arguments
    void *last_host_ctx;
    xUSBH_HCD_Event_Callback_t last_callback;
    xUSBH_Transfer_t *last_transfer;
    uint8_t last_port;
    bool last_port_power_enable;

    // Configurable state
    xUSBH_HCD_Port_Status_t port_status; // returned verbatim by get_port_status
    uint32_t frame_number;               // returned by get_frame_number

    // Return value control - all default to xRETURN_OK via reset_fake_hcd()
    xRETURN_t init_return;
    xRETURN_t deinit_return;
    xRETURN_t start_return;
    xRETURN_t stop_return;
    xRETURN_t enable_interrupts_return;
    xRETURN_t disable_interrupts_return;
    xRETURN_t port_power_return;
    xRETURN_t port_reset_return;
    xRETURN_t get_port_status_return;
    xRETURN_t submit_transfer_return;
    xRETURN_t cancel_transfer_return;
} Fake_HCD_Context_t;

static Fake_HCD_Context_t g_fake_hcd;

static const xUSBH_Init_Config_t valid_init_config = {
    .root_port_count = 1U,
};

static xUSBH_Start_Config_t valid_start_config;

// FAKE HCD FUNCTIONS ///////////////////////////////////////////////////////////

static xRETURN_t fake_hcd_init(void *hcd_ctx, void *host_ctx, xUSBH_HCD_Event_Callback_t callback)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->init_count++;
    fake->last_host_ctx = host_ctx;
    fake->last_callback = callback;
    return fake->init_return;
}

static xRETURN_t fake_hcd_deinit(void *hcd_ctx)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->deinit_count++;
    return fake->deinit_return;
}

static xRETURN_t fake_hcd_start(void *hcd_ctx)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->start_count++;
    return fake->start_return;
}

static xRETURN_t fake_hcd_stop(void *hcd_ctx)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->stop_count++;
    return fake->stop_return;
}

static xRETURN_t fake_hcd_enable_interrupts(void *hcd_ctx)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->enable_interrupts_count++;
    return fake->enable_interrupts_return;
}

static xRETURN_t fake_hcd_disable_interrupts(void *hcd_ctx)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->disable_interrupts_count++;
    return fake->disable_interrupts_return;
}

static xRETURN_t fake_hcd_port_power(void *hcd_ctx, uint8_t port, bool enable)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->port_power_count++;
    fake->last_port = port;
    fake->last_port_power_enable = enable;
    return fake->port_power_return;
}

static xRETURN_t fake_hcd_port_reset(void *hcd_ctx, uint8_t port)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->port_reset_count++;
    fake->last_port = port;
    return fake->port_reset_return;
}

static xRETURN_t fake_hcd_get_port_status(void *hcd_ctx, uint8_t port, xUSBH_HCD_Port_Status_t *status)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->get_port_status_count++;
    fake->last_port = port;
    *status = fake->port_status;
    return fake->get_port_status_return;
}

static xRETURN_t fake_hcd_submit_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->submit_transfer_count++;
    fake->last_transfer = transfer;
    return fake->submit_transfer_return;
}

static xRETURN_t fake_hcd_cancel_transfer(void *hcd_ctx, xUSBH_Transfer_t *transfer)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->cancel_transfer_count++;
    fake->last_transfer = transfer;
    return fake->cancel_transfer_return;
}

static uint32_t fake_hcd_get_frame_number(void *hcd_ctx)
{
    Fake_HCD_Context_t *fake = (Fake_HCD_Context_t *)hcd_ctx;
    fake->get_frame_number_count++;
    return fake->frame_number;
}

static const xUSBH_HCD_Ops_t fake_hcd_ops = {
    .init = fake_hcd_init,
    .deinit = fake_hcd_deinit,
    .start = fake_hcd_start,
    .stop = fake_hcd_stop,
    .enable_interrupts = fake_hcd_enable_interrupts,
    .disable_interrupts = fake_hcd_disable_interrupts,
    .port_power = fake_hcd_port_power,
    .port_reset = fake_hcd_port_reset,
    .get_port_status = fake_hcd_get_port_status,
    .submit_transfer = fake_hcd_submit_transfer,
    .cancel_transfer = fake_hcd_cancel_transfer,
    .get_frame_number = fake_hcd_get_frame_number,
};

// SHARED HELPERS ///////////////////////////////////////////////////////////////

// Reset all fake HCD state.  Call at the start of each setUp().
// Defaults: all call counts = 0, all returns = xRETURN_OK,
//           port connected + enabled + high-speed, frame_number = 42.
static inline void reset_fake_hcd(void)
{
    (void)memset(&g_fake_hcd, 0, sizeof(g_fake_hcd));
    g_fake_hcd.init_return = xRETURN_OK;
    g_fake_hcd.deinit_return = xRETURN_OK;
    g_fake_hcd.start_return = xRETURN_OK;
    g_fake_hcd.stop_return = xRETURN_OK;
    g_fake_hcd.enable_interrupts_return = xRETURN_OK;
    g_fake_hcd.disable_interrupts_return = xRETURN_OK;
    g_fake_hcd.port_power_return = xRETURN_OK;
    g_fake_hcd.port_reset_return = xRETURN_OK;
    g_fake_hcd.get_port_status_return = xRETURN_OK;
    g_fake_hcd.submit_transfer_return = xRETURN_OK;
    g_fake_hcd.cancel_transfer_return = xRETURN_OK;
    g_fake_hcd.port_status.is_connected = true;
    g_fake_hcd.port_status.is_enabled = true;
    g_fake_hcd.port_status.speed = USB_SPEED_HIGH;
    g_fake_hcd.frame_number = 42U;
    valid_start_config.hcd_ops = &fake_hcd_ops;
    valid_start_config.hcd_ctx = &g_fake_hcd;
}

// Initialize and start a host context using the shared fake HCD.
static inline void init_and_start_host(xUSBH_Context_t *host)
{
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Init(host, &valid_init_config));
    TEST_ASSERT_EQUAL(xRETURN_OK, xUSBH_Start(host, &valid_start_config));
}

#endif // TEST_XUSBH_HELPERS_H
// EOF /////////////////////////////////////////////////////////////////////////////
