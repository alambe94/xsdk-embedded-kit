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

// @file test_xlogic_core.c
// @brief Host unit tests for the xLOGIC core state machine.
//
// The fake MSRAM (4 KB static array) replaces real hardware MSRAM.
// Tests write directly to fake_ctrl() fields to simulate PRU behaviour.
// The fake_tx_fn() captures bytes sent to the host for inspection.
//
// Tests cover:
//   1. xLOGIC_Init    - null rejection, IDLE state on init.
//   2. xLOGIC_Arm     - IDLE->ARMED transition, PRU CMD_ARM written.
//   3. xLOGIC_Abort   - any state -> IDLE; PRU CMD_HALT written.
//   4. xLOGIC_Poll    - IDLE no-op; ARMED->DONE on STATUS_DONE.
//   5. Poll in DONE   - samples drained, sent (reversed), core returns to IDLE.
//   6. Poll with overrun - ARMED->ERROR; Abort recovers to IDLE.
//   7. Arm while not IDLE - returns INVALID_STATE.
//   8. TRIGGER_NONE   - trigger_channel still valid after arm.

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "xlogic_core.h"
#include "xlogic_defs.h"
#include "xlogic_return.h"
#include "xlogic_pru.h"
#include "xlogic_transport.h"

#include "test_helpers.h"

// Overriding weak xassert_system_halt to prevent hanging on assertions in unit tests
void xassert_system_halt(void)
{
    // Do nothing: allows the test to continue and verify the return value
}

// FIXTURES ////////////////////////////////////////////////////////////////////////

static xLOGIC_PRU_Context_t s_pru;
static xLOGIC_Transport_Context_t s_transport;
static xLOGIC_Core_Context_t s_core;

static xLOGIC_Config_t default_config(void)
{
    xLOGIC_Config_t cfg;

    cfg.sample_rate = 100000000U; // 100 MHz
    cfg.sample_count = 8U;
    cfg.channel_mask = 0xFFU;
    cfg.trigger_mode = xLOGIC_TRIGGER_NONE;
    cfg.trigger_mask = 0U;
    cfg.trigger_value = 0U;
    cfg.trigger_channel = 0U;
    cfg.trigger_edge = xLOGIC_TRIGGER_EDGE_RISING;

    return cfg;
}

void setUp(void)
{
    reset_fake_msram();
    reset_tx_capture();
    reset_sample_buf();

    xRETURN_t ret;

    ret = xLOGIC_PRU_Init(&s_pru, s_fake_msram, FAKE_MSRAM_BYTES);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);

    ret = xLOGIC_Transport_Init(&s_transport, fake_tx_fn, NULL);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);

    ret = xLOGIC_Init(&s_core, &s_pru, &s_transport, s_sample_buf, TEST_SAMPLE_BUF_BYTES);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
}

void tearDown(void)
{
}

// TESTS ///////////////////////////////////////////////////////////////////////////

void test_core_idle_on_init(void)
{
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_IDLE, (uint32_t)xLOGIC_Get_State(&s_core));
}

void test_arm_transitions_to_armed(void)
{
    xLOGIC_Config_t cfg = default_config();
    xRETURN_t ret = xLOGIC_Arm(&s_core, &cfg);

    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_ARMED, (uint32_t)xLOGIC_Get_State(&s_core));
}

void test_arm_writes_cmd_arm_to_pru(void)
{
    xLOGIC_Config_t cfg = default_config();
    (void)xLOGIC_Arm(&s_core, &cfg);

    TEST_ASSERT_EQUAL_UINT32(xLOGIC_PRU_CMD_ARM, fake_ctrl()->cmd);
}

void test_arm_while_not_idle_returns_error(void)
{
    xLOGIC_Config_t cfg = default_config();

    (void)xLOGIC_Arm(&s_core, &cfg);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_ARMED, (uint32_t)xLOGIC_Get_State(&s_core));

    // Second arm attempt while ARMED should fail
    xRETURN_t ret = xLOGIC_Arm(&s_core, &cfg);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xLOGIC_INVALID_STATE, ret);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_ARMED, (uint32_t)xLOGIC_Get_State(&s_core));
}

void test_abort_from_armed_returns_to_idle(void)
{
    xLOGIC_Config_t cfg = default_config();
    (void)xLOGIC_Arm(&s_core, &cfg);

    xRETURN_t ret = xLOGIC_Abort(&s_core);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_IDLE, (uint32_t)xLOGIC_Get_State(&s_core));
}

void test_abort_writes_halt_to_pru(void)
{
    xLOGIC_Config_t cfg = default_config();
    (void)xLOGIC_Arm(&s_core, &cfg);
    (void)xLOGIC_Abort(&s_core);

    TEST_ASSERT_EQUAL_UINT32(xLOGIC_PRU_CMD_HALT, fake_ctrl()->cmd);
}

void test_poll_idle_is_noop(void)
{
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_IDLE, (uint32_t)xLOGIC_Get_State(&s_core));

    xRETURN_t ret = xLOGIC_Poll(&s_core);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_IDLE, (uint32_t)xLOGIC_Get_State(&s_core));
}

void test_poll_armed_transitions_to_done_on_pru_status_done(void)
{
    xLOGIC_Config_t cfg = default_config();
    (void)xLOGIC_Arm(&s_core, &cfg);

    // Simulate PRU completing capture of 8 bytes
    inject_pattern(0xAAU, 8U);

    xRETURN_t ret = xLOGIC_Poll(&s_core); // ARMED -> DONE
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);

    ret = xLOGIC_Poll(&s_core); // DONE -> IDLE (drains samples, sends)
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_IDLE, (uint32_t)xLOGIC_Get_State(&s_core));
}

void test_poll_done_sends_samples_reversed(void)
{
    // Prepare 4 distinct sample bytes so we can verify reversal
    static const uint8_t samples[4U] = {0x01U, 0x02U, 0x03U, 0x04U};

    xLOGIC_Config_t cfg = default_config();
    cfg.sample_count = 4U;
    (void)xLOGIC_Arm(&s_core, &cfg);

    inject_samples(samples, 4U);

    (void)xLOGIC_Poll(&s_core); // ARMED -> DONE
    (void)xLOGIC_Poll(&s_core); // DONE -> IDLE

    // SUMP sends newest-first: expect 0x04, 0x03, 0x02, 0x01
    TEST_ASSERT_EQUAL_UINT32(4U, s_tx_len);
    TEST_ASSERT_EQUAL_UINT8(0x04U, s_tx_capture[0U]);
    TEST_ASSERT_EQUAL_UINT8(0x03U, s_tx_capture[1U]);
    TEST_ASSERT_EQUAL_UINT8(0x02U, s_tx_capture[2U]);
    TEST_ASSERT_EQUAL_UINT8(0x01U, s_tx_capture[3U]);
}

void test_poll_overrun_transitions_to_error(void)
{
    xLOGIC_Config_t cfg = default_config();
    (void)xLOGIC_Arm(&s_core, &cfg);

    // Simulate PRU overrun
    set_pru_status(xLOGIC_PRU_STATUS_OVERRUN);

    xRETURN_t ret = xLOGIC_Poll(&s_core);
    TEST_ASSERT_EQUAL(xRETURN_xERR_xLOGIC_OVERRUN, ret);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_ERROR, (uint32_t)xLOGIC_Get_State(&s_core));
}

void test_abort_from_error_returns_to_idle(void)
{
    xLOGIC_Config_t cfg = default_config();
    (void)xLOGIC_Arm(&s_core, &cfg);
    set_pru_status(xLOGIC_PRU_STATUS_OVERRUN);
    (void)xLOGIC_Poll(&s_core);

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_ERROR, (uint32_t)xLOGIC_Get_State(&s_core));

    xRETURN_t ret = xLOGIC_Abort(&s_core);
    TEST_ASSERT_EQUAL(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_IDLE, (uint32_t)xLOGIC_Get_State(&s_core));
}

void test_null_core_ctx_returns_error(void)
{
    xLOGIC_Config_t cfg = default_config();

    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xLOGIC_Arm(NULL, &cfg));
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xLOGIC_Abort(NULL));
    TEST_ASSERT_NOT_EQUAL(xRETURN_OK, xLOGIC_Poll(NULL));

    TEST_ASSERT_EQUAL_UINT32((uint32_t)xLOGIC_STATE_ERROR, (uint32_t)xLOGIC_Get_State(NULL));
}

// RUNNER //////////////////////////////////////////////////////////////////////////

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_core_idle_on_init);
    RUN_TEST(test_arm_transitions_to_armed);
    RUN_TEST(test_arm_writes_cmd_arm_to_pru);
    RUN_TEST(test_arm_while_not_idle_returns_error);
    RUN_TEST(test_abort_from_armed_returns_to_idle);
    RUN_TEST(test_abort_writes_halt_to_pru);
    RUN_TEST(test_poll_idle_is_noop);
    RUN_TEST(test_poll_armed_transitions_to_done_on_pru_status_done);
    RUN_TEST(test_poll_done_sends_samples_reversed);
    RUN_TEST(test_poll_overrun_transitions_to_error);
    RUN_TEST(test_abort_from_error_returns_to_idle);
    RUN_TEST(test_null_core_ctx_returns_error);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
