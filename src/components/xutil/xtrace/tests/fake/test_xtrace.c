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

// @file test_xtrace.c
// @brief Host unit tests for xTrace v5 LEB128 recorder core.
//

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "xtrace.h"
#include "xtrace_registry.h"

// -- Test fixtures -------------------------------------------------------------

// 256 bytes holds ~80+ small LEB128 events (3 bytes each at typical sizes).
#define TEST_BUF_BYTES 256U

static uint8_t s_buf[TEST_BUF_BYTES];
static xTRACE_Context_t s_ctx;
static uint32_t s_tick;

static xTRACE_Time_t fake_timestamp(void *ctx)
{
    (void)ctx;
    return s_tick++;
}

// Fake transport - records bytes written; supports failure / partial-write.
typedef struct
{
    uint8_t output[512U];
    size_t bytes_written;
    bool fail_on_write;
    size_t partial_limit; // cap each write call to this many bytes (0 = no cap)
    bool stall_write;     // return OK but write 0 bytes
    bool overrun_write;   // return OK but claim written > len
} fake_transport_ctx_t;

static fake_transport_ctx_t s_transport_ctx;

static xRETURN_t fake_write(void *ctx, const uint8_t *buf, size_t len, size_t *written)
{
    fake_transport_ctx_t *ftx = (fake_transport_ctx_t *)ctx;

    if (ftx->fail_on_write)
    {
        *written = 0U;
        return xRETURN_xERR_xTRACE_TRANSPORT;
    }
    if (ftx->stall_write)
    {
        *written = 0U;
        return xRETURN_OK;
    }
    if (ftx->overrun_write)
    {
        *written = len + 1U;
        return xRETURN_OK;
    }

    size_t to_copy = len;
    if ((ftx->partial_limit > 0U) && (to_copy > ftx->partial_limit))
    {
        to_copy = ftx->partial_limit;
    }
    memcpy(&ftx->output[ftx->bytes_written], buf, to_copy);
    ftx->bytes_written += to_copy;
    *written = to_copy;
    return xRETURN_OK;
}

static const xTRACE_Transport_t s_transport = {.write = fake_write};

static void reset_transport(void)
{
    memset(&s_transport_ctx, 0, sizeof(s_transport_ctx));
}

static xTRACE_Config_t make_config(bool enabled)
{
    xTRACE_Config_t cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.buffer = s_buf;
    cfg.capacity_bytes = TEST_BUF_BYTES;
    cfg.timestamp_fn = fake_timestamp;
    cfg.timestamp_ctx = NULL;
    cfg.timestamp_hz = 1000000U;
    cfg.is_enabled = enabled;
    return cfg;
}

// LEB128 decode helper for test verification.
static uint32_t leb128_decode(const uint8_t *buf, uint32_t *pos)
{
    uint32_t value = 0U;
    uint32_t shift = 0U;
    while (true)
    {
        uint8_t byte = buf[(*pos)++];
        value |= ((uint32_t)(byte & 0x7FU)) << shift;
        shift += 7U;
        if ((byte & 0x80U) == 0U)
        {
            break;
        }
    }
    return value;
}

void setUp(void)
{
    memset(s_buf, 0, sizeof(s_buf));
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_tick = 0U;
    reset_transport();
}

void tearDown(void)
{
}

// -- Init validation -----------------------------------------------------------

void test_init_rejects_null_context(void)
{
    xTRACE_Config_t cfg = make_config(true);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NULL_POINTER, xTRACE_Init(NULL, &cfg, NULL, NULL));
}

void test_init_rejects_null_config(void)
{
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NULL_POINTER, xTRACE_Init(&s_ctx, NULL, NULL, NULL));
}

void test_init_rejects_null_buffer(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.buffer = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NULL_POINTER, xTRACE_Init(&s_ctx, &cfg, NULL, NULL));
}

void test_init_rejects_null_timestamp_fn(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_fn = NULL;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NULL_POINTER, xTRACE_Init(&s_ctx, &cfg, NULL, NULL));
}

void test_init_rejects_capacity_below_minimum(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.capacity_bytes = 8U; // minimum is 16
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_INVALID_ARG, xTRACE_Init(&s_ctx, &cfg, NULL, NULL));
}

void test_init_rejects_zero_timestamp_frequency(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 0U;
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_INVALID_ARG, xTRACE_Init(&s_ctx, &cfg, NULL, NULL));
}

void test_init_rejects_transport_with_null_write(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Transport_t transport;
    memset(&transport, 0, sizeof(transport));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NULL_POINTER, xTRACE_Init(&s_ctx, &cfg, &transport, NULL));
}

void test_registry_base_ranges_are_reserved(void)
{
    TEST_ASSERT_EQUAL_UINT32(0x00U, xTRACE_BASE_CORE);
    TEST_ASSERT_EQUAL_UINT32(0x20U, xTRACE_BASE_xRTOS);
    TEST_ASSERT_EQUAL_UINT32(0x40U, xTRACE_BASE_xFS);
    TEST_ASSERT_EQUAL_UINT32(0x60U, xTRACE_BASE_xUSB);
    TEST_ASSERT_EQUAL_UINT32(0x80U, xTRACE_BASE_xUSBH);
    TEST_ASSERT_EQUAL_UINT32(0xA0U, xTRACE_BASE_xUART);
    TEST_ASSERT_EQUAL_UINT32(0xC0U, xTRACE_BASE_xI2C);
    TEST_ASSERT_EQUAL_UINT32(0xE0U, xTRACE_BASE_xSPI);
    TEST_ASSERT_EQUAL_UINT32(0x100U, xTRACE_BASE_xGPIO);
    TEST_ASSERT_EQUAL_UINT32(0x120U, xTRACE_BASE_xTIMER);
    TEST_ASSERT_EQUAL_UINT32(0x140U, xTRACE_BASE_USER);

    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_CORE + 0x20U, xTRACE_BASE_xRTOS);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_xRTOS + 0x20U, xTRACE_BASE_xFS);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_xFS + 0x20U, xTRACE_BASE_xUSB);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_xUSB + 0x20U, xTRACE_BASE_xUSBH);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_xUSBH + 0x20U, xTRACE_BASE_xUART);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_xUART + 0x20U, xTRACE_BASE_xI2C);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_xI2C + 0x20U, xTRACE_BASE_xSPI);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_xSPI + 0x20U, xTRACE_BASE_xGPIO);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_xGPIO + 0x20U, xTRACE_BASE_xTIMER);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_BASE_xTIMER + 0x20U, xTRACE_BASE_USER);
}

void test_init_succeeds_with_valid_config(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xRETURN_t ret = xTRACE_Init(&s_ctx, &cfg, &s_transport, &s_transport_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_TRUE(s_ctx.is_initialized);
    TEST_ASSERT_TRUE(s_ctx.is_enabled);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.read_pos);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.dropped_count);
    TEST_ASSERT_FALSE(s_ctx.is_gap_pending);
}

// -- BOOT record ---------------------------------------------------------------

void test_init_emits_boot_record(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    // write_pos must be > 0: BOOT record was written.
    TEST_ASSERT_GREATER_THAN_UINT32(0U, s_ctx.write_pos);

    // Read and verify the boot record starting with length prefix.
    uint32_t pos = 0;
    uint32_t len = leb128_decode(s_buf, &pos);
    uint32_t ev = leb128_decode(s_buf, &pos);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, len);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_EV_BOOT, ev);
}

void test_init_disabled_does_not_emit_boot(void)
{
    xTRACE_Config_t cfg = make_config(false);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    // No BOOT record when disabled.
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.write_pos);
}

// -- LEB128 encoding -----------------------------------------------------------

void test_emit1_small_values_fit_in_three_bytes(void)
{
    // With event_id=0x20 (<128), delta=0, param=0x05:
    // all three fit in 1 LEB128 byte each -> 3 bytes for the event payload.
    // Length prefix takes 1 byte, making it 4 bytes total.
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    uint32_t pos_before = s_ctx.write_pos;

    // Force delta = 0 by syncing timestamp
    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, 0x20U, 0x05U);

    // prefix(1) + event(1) + delta(1) + param(1) = 4 bytes
    TEST_ASSERT_EQUAL_UINT32(pos_before + 4U, s_ctx.write_pos);

    // Decode and verify
    uint32_t pos = pos_before;
    uint32_t len = leb128_decode(s_buf, &pos);
    uint32_t ev = leb128_decode(s_buf, &pos);
    uint32_t delta = leb128_decode(s_buf, &pos);
    uint32_t param = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(3U, len);
    TEST_ASSERT_EQUAL_UINT32(0x20U, ev);
    TEST_ASSERT_EQUAL_UINT32(0U, delta);
    TEST_ASSERT_EQUAL_UINT32(0x05U, param);
}

void test_emit1_large_param_uses_two_bytes(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    uint32_t pos_before = s_ctx.write_pos;

    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, 0x10U, 0x80U); // 0x80 = 128, needs 2 LEB128 bytes

    // prefix(1) + event(1) + delta(1) + param(2) = 5 bytes
    TEST_ASSERT_EQUAL_UINT32(pos_before + 5U, s_ctx.write_pos);

    uint32_t pos = pos_before;
    uint32_t len = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(4U, len);
    leb128_decode(s_buf, &pos); // skip event_id
    leb128_decode(s_buf, &pos); // skip delta
    uint32_t param = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(0x80U, param);
}

void test_emit2_writes_two_params(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    uint32_t pos_before = s_ctx.write_pos;

    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit2(&s_ctx, 0x12U, 0x01U, 0x02U);

    // prefix(1) + event(1) + delta(1) + param1(1) + param2(1) = 5 bytes
    TEST_ASSERT_EQUAL_UINT32(pos_before + 5U, s_ctx.write_pos);

    uint32_t pos = pos_before;
    uint32_t len = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(4U, len);
    uint32_t ev = leb128_decode(s_buf, &pos);
    leb128_decode(s_buf, &pos); // delta
    uint32_t a = leb128_decode(s_buf, &pos);
    uint32_t b = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(0x12U, ev);
    TEST_ASSERT_EQUAL_UINT32(0x01U, a);
    TEST_ASSERT_EQUAL_UINT32(0x02U, b);
}

void test_emit3_writes_three_params(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    uint32_t pos_before = s_ctx.write_pos;

    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit3(&s_ctx, 0x18U, 10U, 20U, 30U);

    // prefix(1) + event(1) + delta(1) + param1(1) + param2(1) + param3(1) = 6 bytes
    TEST_ASSERT_EQUAL_UINT32(pos_before + 6U, s_ctx.write_pos);

    uint32_t pos = pos_before;
    uint32_t len = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(5U, len);
    uint32_t ev = leb128_decode(s_buf, &pos);
    leb128_decode(s_buf, &pos); // delta
    uint32_t a = leb128_decode(s_buf, &pos);
    uint32_t b = leb128_decode(s_buf, &pos);
    uint32_t c = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(0x18U, ev);
    TEST_ASSERT_EQUAL_UINT32(10U, a);
    TEST_ASSERT_EQUAL_UINT32(20U, b);
    TEST_ASSERT_EQUAL_UINT32(30U, c);
}

void test_emit_name_writes_printable_ascii_bytes(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    uint32_t pos_before = s_ctx.write_pos;

    s_tick = s_ctx.last_timestamp;
    xTRACE_EmitName(&s_ctx, 0x3BU, 1U, 7U, "Queue 7");

    uint32_t pos = pos_before;
    (void)leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(0x3BU, leb128_decode(s_buf, &pos));
    TEST_ASSERT_EQUAL_UINT32(0U, leb128_decode(s_buf, &pos));
    TEST_ASSERT_EQUAL_UINT32(1U, leb128_decode(s_buf, &pos));
    TEST_ASSERT_EQUAL_UINT32(7U, leb128_decode(s_buf, &pos));
    TEST_ASSERT_EQUAL_UINT32(7U, leb128_decode(s_buf, &pos));
    TEST_ASSERT_EQUAL_MEMORY("Queue 7", &s_buf[pos], 7U);
}

void test_emit_name_rejects_non_ascii_bytes(void)
{
    static const char invalid_name[] = {(char)0x80, '\0'};
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    uint32_t pos_before = s_ctx.write_pos;

    xTRACE_EmitName(&s_ctx, 0x3BU, 1U, 7U, invalid_name);

    TEST_ASSERT_EQUAL_UINT32(pos_before, s_ctx.write_pos);
}

void test_emit0_writes_event_id_and_delta_only(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    uint32_t pos_before = s_ctx.write_pos;

    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit0(&s_ctx, 0x19U);

    // prefix(1) + event(1) + delta(1) = 3 bytes
    TEST_ASSERT_EQUAL_UINT32(pos_before + 3U, s_ctx.write_pos);

    uint32_t pos = pos_before;
    uint32_t len = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(2U, len);
    uint32_t ev = leb128_decode(s_buf, &pos);
    uint32_t delta = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(0x19U, ev);
    TEST_ASSERT_EQUAL_UINT32(0U, delta);
}

// -- Delta timestamp -----------------------------------------------------------

void test_delta_timestamp_increases_between_events(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    uint32_t pos_before = s_ctx.write_pos;

    // First event: some delta
    xTRACE_Emit0(&s_ctx, 0x19U);
    uint32_t pos = pos_before;
    (void)leb128_decode(s_buf, &pos); // skip prefix
    (void)leb128_decode(s_buf, &pos); // skip event_id
    uint32_t delta1 = leb128_decode(s_buf, &pos);

    // Second event: larger delta
    xTRACE_Emit0(&s_ctx, 0x19U);
    (void)leb128_decode(s_buf, &pos); // skip prefix
    (void)leb128_decode(s_buf, &pos); // skip event_id
    uint32_t delta2 = leb128_decode(s_buf, &pos);

    // Both deltas >= 0; second is non-zero
    TEST_ASSERT_GREATER_THAN_UINT32(0U, delta1 + delta2);
}

// -- Overflow: drop and GAP ----------------------------------------------------

void test_overflow_increments_dropped_count(void)
{
    xTRACE_Config_t cfg = make_config(true);
    // Tiny buffer: 16 bytes. BOOT takes ~5 bytes. Only a few events fit.
    static uint8_t small_buf[16U];
    cfg.buffer = small_buf;
    cfg.capacity_bytes = 16U;
    memset(small_buf, 0, sizeof(small_buf));
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    // Fill until overflow
    for (uint32_t i = 0U; i < 20U; i++)
    {
        xTRACE_Emit1(&s_ctx, 0x20U, i);
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0U, s_ctx.dropped_count);
    TEST_ASSERT_TRUE(s_ctx.is_gap_pending);
}

void test_gap_record_emitted_after_overflow_recovery(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    // Force is_gap_pending manually (simulating a prior overflow)
    s_ctx.is_gap_pending = true;
    s_ctx.dropped_count = 7U;

    uint32_t pos_before = s_ctx.write_pos;
    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit0(&s_ctx, 0x20U); // should prepend GAP then write the event

    // GAP record: [prefix][0x00][delta][dropped_count]
    uint32_t pos = pos_before;
    uint32_t gap_len = leb128_decode(s_buf, &pos);
    uint32_t gap_ev = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(0x00U, gap_ev); // xTRACE_EV_GAP
    uint32_t gap_delta = leb128_decode(s_buf, &pos);
    uint32_t gap_dropped = leb128_decode(s_buf, &pos);
    (void)gap_delta;
    TEST_ASSERT_EQUAL_UINT32(7U, gap_dropped);
    TEST_ASSERT_EQUAL_UINT32(3U, gap_len); // [0x00] + [0] + [7]

    // GAP clears state
    TEST_ASSERT_FALSE(s_ctx.is_gap_pending);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.dropped_count);

    // Normal event follows GAP
    uint32_t ev_len = leb128_decode(s_buf, &pos);
    (void)ev_len;
    uint32_t ev = leb128_decode(s_buf, &pos);
    TEST_ASSERT_EQUAL_UINT32(0x20U, ev);
}

// -- Disabled tracing ----------------------------------------------------------

void test_disabled_tracing_does_not_write_events(void)
{
    xTRACE_Config_t cfg = make_config(false);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    uint32_t pos_after_init = s_ctx.write_pos; // 0 (no BOOT when disabled)

    xTRACE_Emit1(&s_ctx, 0x20U, 0xAAU);

    TEST_ASSERT_EQUAL_UINT32(pos_after_init, s_ctx.write_pos);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.dropped_count);
}

void test_emit1_on_null_context_is_noop(void)
{
    xTRACE_Emit1(NULL, 0x20U, 0U); // must not crash
}

void test_emit1_on_uninitialized_context_is_noop(void)
{
    // s_ctx is zero-initialised - is_initialized is false
    uint32_t pos = s_ctx.write_pos;
    xTRACE_Emit1(&s_ctx, 0x20U, 0U);
    TEST_ASSERT_EQUAL_UINT32(pos, s_ctx.write_pos);
}

// -- Dynamic filtering and overwrite tests -------------------------------------

void test_dynamic_runtime_filtering(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    // Disable event 0x20
    xTRACE_Filter_Disable_Event(&s_ctx, 0x20U);

    uint32_t pos_before = s_ctx.write_pos;
    xTRACE_Emit1(&s_ctx, 0x20U, 0xAAU); // Should be filtered out
    TEST_ASSERT_EQUAL_UINT32(pos_before, s_ctx.write_pos);

    // Enable event 0x20
    xTRACE_Filter_Enable_Event(&s_ctx, 0x20U);
    xTRACE_Emit1(&s_ctx, 0x20U, 0xAAU); // Should be emitted
    TEST_ASSERT_GREATER_THAN_UINT32(pos_before, s_ctx.write_pos);

    // Disable all
    pos_before = s_ctx.write_pos;
    xTRACE_Filter_Disable_All(&s_ctx);
    xTRACE_Emit1(&s_ctx, 0x20U, 0xAAU); // Filtered out
    xTRACE_Emit1(&s_ctx, 0x21U, 0xBBU); // Filtered out
    TEST_ASSERT_EQUAL_UINT32(pos_before, s_ctx.write_pos);

    // Enable all
    xTRACE_Filter_Enable_All(&s_ctx);
    xTRACE_Emit1(&s_ctx, 0x20U, 0xAAU); // Emitted
    TEST_ASSERT_GREATER_THAN_UINT32(pos_before, s_ctx.write_pos);
}

void test_module_filter_enable_passes_events_in_block(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    xTRACE_Filter_Disable_All(&s_ctx);

    xTRACE_Filter_Enable_Module(&s_ctx, xTRACE_BASE_xRTOS);

    // Event inside xRTOS block (0x20) should now emit.
    uint32_t pos_before = s_ctx.write_pos;
    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, xTRACE_BASE_xRTOS, 0xAAU);
    TEST_ASSERT_GREATER_THAN_UINT32(pos_before, s_ctx.write_pos);

    // Event outside xRTOS block (0x40, xFS) should remain filtered.
    pos_before = s_ctx.write_pos;
    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, xTRACE_BASE_xFS, 0xBBU);
    TEST_ASSERT_EQUAL_UINT32(pos_before, s_ctx.write_pos);
}

void test_module_filter_disable_blocks_events_in_block(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    // Confirm xRTOS event emits before disable.
    uint32_t pos_before = s_ctx.write_pos;
    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, xTRACE_BASE_xRTOS, 0xAAU);
    TEST_ASSERT_GREATER_THAN_UINT32(pos_before, s_ctx.write_pos);

    xTRACE_Filter_Disable_Module(&s_ctx, xTRACE_BASE_xRTOS);

    // xRTOS event should now be filtered.
    pos_before = s_ctx.write_pos;
    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, xTRACE_BASE_xRTOS, 0xBBU);
    TEST_ASSERT_EQUAL_UINT32(pos_before, s_ctx.write_pos);

    // Event in a different block (xFS) should still emit.
    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, xTRACE_BASE_xFS, 0xCCU);
    TEST_ASSERT_GREATER_THAN_UINT32(pos_before, s_ctx.write_pos);
}

void test_module_filter_disable_core_preserves_gap_boot_time_sync(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    xTRACE_Filter_Disable_Module(&s_ctx, xTRACE_BASE_CORE);

    // GAP/BOOT/TIME_SYNC bits must survive the disable.
    TEST_ASSERT_BITS_HIGH(1U << xTRACE_EV_GAP, s_ctx.enabled_events_mask[0]);
    TEST_ASSERT_BITS_HIGH(1U << xTRACE_EV_BOOT, s_ctx.enabled_events_mask[0]);
    TEST_ASSERT_BITS_HIGH(1U << xTRACE_EV_TIME_SYNC, s_ctx.enabled_events_mask[0]);

    // All other core bits must be cleared.
    TEST_ASSERT_BITS_LOW(1U << 3U, s_ctx.enabled_events_mask[0]);
    TEST_ASSERT_BITS_LOW(1U << 31U, s_ctx.enabled_events_mask[0]);
}

void test_filter_disable_all_preserves_core_protected_events(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    xTRACE_Filter_Disable_All(&s_ctx);

    // GAP/BOOT/TIME_SYNC bits must survive a full disable.
    TEST_ASSERT_BITS_HIGH(1U << xTRACE_EV_GAP, s_ctx.enabled_events_mask[0]);
    TEST_ASSERT_BITS_HIGH(1U << xTRACE_EV_BOOT, s_ctx.enabled_events_mask[0]);
    TEST_ASSERT_BITS_HIGH(1U << xTRACE_EV_TIME_SYNC, s_ctx.enabled_events_mask[0]);

    // Non-core words must be fully cleared.
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.enabled_events_mask[1]);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.enabled_events_mask[2]);
}

void test_filter_module_null_context_is_noop(void)
{
    xTRACE_Filter_Enable_Module(NULL, xTRACE_BASE_xRTOS);
    xTRACE_Filter_Disable_Module(NULL, xTRACE_BASE_xRTOS);
}

void test_overwrite_overrun_policy(void)
{
    xTRACE_Config_t cfg = make_config(true);
    cfg.timestamp_hz = 1U;
    cfg.overrun_policy = xTRACE_OVERRUN_OVERWRITE;

    // Tiny buffer to force wraps easily: 32 bytes
    static uint8_t overwrite_buf[32U];
    cfg.buffer = overwrite_buf;
    cfg.capacity_bytes = 32U;

    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    // Clear buffer/positions so we start clean (ignoring BOOT record for simple verification)
    s_ctx.write_pos = 0U;
    s_ctx.read_pos = 0U;
    memset(overwrite_buf, 0, sizeof(overwrite_buf));

    // Write 5 records of 3 bytes payload + 1 byte len = 4 bytes each.
    // Total space = 32 - 1 = 31 bytes.
    // So 5 records * 4 bytes = 20 bytes. Should fit easily.
    for (uint32_t i = 0U; i < 5U; i++)
    {
        s_tick = s_ctx.last_timestamp;
        xTRACE_Emit1(&s_ctx, 0x20U, i);
    }

    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.dropped_count);

    // Write 5 more. This will wrap around and overwrite the first few records.
    for (uint32_t i = 5U; i < 10U; i++)
    {
        s_tick = s_ctx.last_timestamp;
        xTRACE_Emit1(&s_ctx, 0x20U, i);
    }

    // Loss is reported in-stream and no loss remains pending in the context.
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.dropped_count);
    TEST_ASSERT_FALSE(s_ctx.is_gap_pending);

    // The read_pos should have advanced past the overwritten records.
    TEST_ASSERT_GREATER_THAN_UINT32(0U, s_ctx.read_pos);

    // Let's flush and verify we only get the latest records.
    fake_transport_ctx_t trans_ctx;
    memset(&trans_ctx, 0, sizeof(trans_ctx));
    s_ctx.transport = &s_transport;
    s_ctx.transport_ctx = &trans_ctx;

    xRETURN_t ret = xTRACE_Flush(&s_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_GREATER_THAN_UINT32(0U, trans_ctx.bytes_written);

    // A timestamp-correct overwrite segment starts with BOOT followed by GAP.
    uint32_t pos = 0;
    (void)leb128_decode(trans_ctx.output, &pos);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_EV_BOOT, leb128_decode(trans_ctx.output, &pos));
    (void)leb128_decode(trans_ctx.output, &pos);
    TEST_ASSERT_EQUAL_UINT32(cfg.timestamp_hz, leb128_decode(trans_ctx.output, &pos));

    (void)leb128_decode(trans_ctx.output, &pos);
    TEST_ASSERT_EQUAL_UINT32(xTRACE_EV_GAP, leb128_decode(trans_ctx.output, &pos));
    TEST_ASSERT_EQUAL_UINT32(0U, leb128_decode(trans_ctx.output, &pos));
    TEST_ASSERT_GREATER_THAN_UINT32(0U, leb128_decode(trans_ctx.output, &pos));

    // Remaining event values are contiguous and end in the latest value.
    uint32_t last_val = 0xFFFFU;
    uint32_t count = 0;
    while (pos < trans_ctx.bytes_written)
    {
        uint32_t len = leb128_decode(trans_ctx.output, &pos);
        (void)len;
        uint32_t ev = leb128_decode(trans_ctx.output, &pos);
        uint32_t delta = leb128_decode(trans_ctx.output, &pos);
        (void)delta;
        uint32_t param = leb128_decode(trans_ctx.output, &pos);

        TEST_ASSERT_EQUAL_UINT32(0x20U, ev);
        if (last_val != 0xFFFFU)
        {
            TEST_ASSERT_EQUAL_UINT32(last_val + 1U, param);
        }
        last_val = param;
        count++;
    }
    // We should have successfully read some records, and the last one must be 9.
    TEST_ASSERT_GREATER_THAN_UINT32(0U, count);
    TEST_ASSERT_EQUAL_UINT32(9U, last_val);
}

// -- Flush ---------------------------------------------------------------------

void test_flush_drains_all_bytes_to_transport(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, &s_transport, &s_transport_ctx);

    xTRACE_Emit1(&s_ctx, 0x20U, 0x01U);
    xTRACE_Emit1(&s_ctx, 0x21U, 0x02U);

    uint32_t bytes_to_flush = s_ctx.write_pos; // includes BOOT + 2 events
    xRETURN_t ret = xTRACE_Flush(&s_ctx);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(bytes_to_flush, s_transport_ctx.bytes_written);
    TEST_ASSERT_EQUAL_UINT32(s_ctx.read_pos, s_ctx.write_pos); // buffer empty
}

void test_flush_handles_partial_write(void)
{
    // In v2, Flush loops until empty or stalled.
    // partial_limit=3 means each write call sends 3 bytes, but the loop
    // continues on each successful partial - draining everything in one
    // Flush call.  Verify the loop drains correctly and content is intact.
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, &s_transport, &s_transport_ctx);

    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, 0x20U, 0x01U);
    uint32_t total = s_ctx.write_pos;

    s_transport_ctx.partial_limit = 3U; // 3 bytes per write call

    xRETURN_t ret = xTRACE_Flush(&s_ctx);

    // Loop drains everything despite the 3-byte cap per call.
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(total, s_transport_ctx.bytes_written);
    TEST_ASSERT_EQUAL_UINT32(s_ctx.read_pos, s_ctx.write_pos); // fully empty
}

void test_flush_handles_transport_failure(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, &s_transport, &s_transport_ctx);

    xTRACE_Emit1(&s_ctx, 0x20U, 0U);
    s_transport_ctx.fail_on_write = true;

    xRETURN_t ret = xTRACE_Flush(&s_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_TRANSPORT, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.read_pos); // no progress
}

void test_flush_transport_overrun_returns_transport_error(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, &s_transport, &s_transport_ctx);

    xTRACE_Emit1(&s_ctx, 0x20U, 0U);
    s_transport_ctx.overrun_write = true;

    xRETURN_t ret = xTRACE_Flush(&s_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_TRANSPORT, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.read_pos);
}

void test_flush_stall_returns_ok_without_progress(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, &s_transport, &s_transport_ctx);

    xTRACE_Emit1(&s_ctx, 0x20U, 0U);
    s_transport_ctx.stall_write = true;

    xRETURN_t ret = xTRACE_Flush(&s_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.read_pos);
    TEST_ASSERT_EQUAL_UINT32(0U, s_transport_ctx.bytes_written);
}

void test_flush_with_no_transport_returns_ok(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    xTRACE_Emit1(&s_ctx, 0x20U, 0U);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, xTRACE_Flush(&s_ctx));
}

void test_flush_rejects_null_context(void)
{
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NULL_POINTER, xTRACE_Flush(NULL));
}

void test_flush_rejects_uninitialized_context(void)
{
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NOT_INITIALIZED, xTRACE_Flush(&s_ctx));
}

void test_flush_handles_wrapped_ring_buffer(void)
{
    // Fill most of the buffer, flush, then write more past the wrap point.
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, &s_transport, &s_transport_ctx);

    // Drain BOOT record first
    xTRACE_Flush(&s_ctx);
    reset_transport();

    // Write events until near the end of the buffer
    while (s_ctx.write_pos < TEST_BUF_BYTES - 20U)
    {
        s_tick = s_ctx.last_timestamp;
        xTRACE_Emit1(&s_ctx, 0x20U, 0x01U);
    }
    xTRACE_Flush(&s_ctx);
    reset_transport();

    // Write events that wrap around
    for (uint32_t i = 0U; i < 10U; i++)
    {
        s_tick = s_ctx.last_timestamp;
        xTRACE_Emit1(&s_ctx, 0x20U, 0x02U);
    }

    xRETURN_t ret = xTRACE_Flush(&s_ctx);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(s_ctx.read_pos, s_ctx.write_pos); // fully drained
    TEST_ASSERT_GREATER_THAN_UINT32(0U, s_transport_ctx.bytes_written);
}

// -- Status --------------------------------------------------------------------

void test_status_reports_capacity_used_bytes_and_dropped(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);

    uint32_t used_after_boot = s_ctx.write_pos;

    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, 0x20U, 0x01U);
    s_tick = s_ctx.last_timestamp;
    xTRACE_Emit1(&s_ctx, 0x21U, 0x02U);

    xTRACE_Status_t st;
    memset(&st, 0, sizeof(st));
    xRETURN_t ret = xTRACE_Get_Status(&s_ctx, &st);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_EQUAL_UINT32(TEST_BUF_BYTES, st.capacity_bytes);
    TEST_ASSERT_GREATER_THAN_UINT32(used_after_boot, st.used_bytes);
    TEST_ASSERT_EQUAL_UINT32(0U, st.dropped_count);
    TEST_ASSERT_TRUE(st.is_initialized);
    TEST_ASSERT_TRUE(st.is_enabled);
}

void test_status_rejects_null_context(void)
{
    xTRACE_Status_t s;
    memset(&s, 0, sizeof(s));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NULL_POINTER, xTRACE_Get_Status(NULL, &s));
}

void test_status_rejects_null_status(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NULL_POINTER, xTRACE_Get_Status(&s_ctx, NULL));
}

void test_status_rejects_uninitialized_context(void)
{
    xTRACE_Status_t st;
    memset(&st, 0, sizeof(st));
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NOT_INITIALIZED, xTRACE_Get_Status(&s_ctx, &st));
}

// -- Deinit --------------------------------------------------------------------

void test_deinit_zeroes_context(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, &s_transport, &s_transport_ctx);
    xTRACE_Emit1(&s_ctx, 0x20U, 0U);

    xRETURN_t ret = xTRACE_Deinit(&s_ctx);

    TEST_ASSERT_EQUAL_UINT32(xRETURN_OK, ret);
    TEST_ASSERT_FALSE(s_ctx.is_initialized);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.write_pos);
    TEST_ASSERT_EQUAL_UINT32(0U, s_ctx.dropped_count);
    TEST_ASSERT_NULL(s_ctx.buffer);
}

void test_deinit_rejects_null_context(void)
{
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NULL_POINTER, xTRACE_Deinit(NULL));
}

void test_deinit_rejects_uninitialized_context(void)
{
    TEST_ASSERT_EQUAL_UINT32(xRETURN_xERR_xTRACE_NOT_INITIALIZED, xTRACE_Deinit(&s_ctx));
}

// -- Context size (informational) ---------------------------------------------

void test_context_uses_byte_buffer(void)
{
    xTRACE_Config_t cfg = make_config(true);
    xTRACE_Init(&s_ctx, &cfg, NULL, NULL);
    // Verify the buffer pointer is exactly the uint8_t* we passed.
    TEST_ASSERT_EQUAL_PTR(s_buf, s_ctx.buffer);
}

// -- Entry point --------------------------------------------------------------

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_init_rejects_null_context);
    RUN_TEST(test_init_rejects_null_config);
    RUN_TEST(test_init_rejects_null_buffer);
    RUN_TEST(test_init_rejects_null_timestamp_fn);
    RUN_TEST(test_init_rejects_capacity_below_minimum);
    RUN_TEST(test_init_rejects_zero_timestamp_frequency);
    RUN_TEST(test_init_rejects_transport_with_null_write);
    RUN_TEST(test_registry_base_ranges_are_reserved);
    RUN_TEST(test_init_succeeds_with_valid_config);

    RUN_TEST(test_init_emits_boot_record);
    RUN_TEST(test_init_disabled_does_not_emit_boot);

    RUN_TEST(test_emit0_writes_event_id_and_delta_only);
    RUN_TEST(test_emit1_small_values_fit_in_three_bytes);
    RUN_TEST(test_emit1_large_param_uses_two_bytes);
    RUN_TEST(test_emit2_writes_two_params);
    RUN_TEST(test_emit3_writes_three_params);
    RUN_TEST(test_emit_name_writes_printable_ascii_bytes);
    RUN_TEST(test_emit_name_rejects_non_ascii_bytes);

    RUN_TEST(test_delta_timestamp_increases_between_events);

    RUN_TEST(test_overflow_increments_dropped_count);
    RUN_TEST(test_gap_record_emitted_after_overflow_recovery);

    RUN_TEST(test_disabled_tracing_does_not_write_events);
    RUN_TEST(test_emit1_on_null_context_is_noop);
    RUN_TEST(test_emit1_on_uninitialized_context_is_noop);

    RUN_TEST(test_dynamic_runtime_filtering);
    RUN_TEST(test_module_filter_enable_passes_events_in_block);
    RUN_TEST(test_module_filter_disable_blocks_events_in_block);
    RUN_TEST(test_module_filter_disable_core_preserves_gap_boot_time_sync);
    RUN_TEST(test_filter_disable_all_preserves_core_protected_events);
    RUN_TEST(test_filter_module_null_context_is_noop);
    RUN_TEST(test_overwrite_overrun_policy);

    RUN_TEST(test_flush_drains_all_bytes_to_transport);
    RUN_TEST(test_flush_handles_partial_write);
    RUN_TEST(test_flush_handles_transport_failure);
    RUN_TEST(test_flush_transport_overrun_returns_transport_error);
    RUN_TEST(test_flush_stall_returns_ok_without_progress);
    RUN_TEST(test_flush_with_no_transport_returns_ok);
    RUN_TEST(test_flush_rejects_null_context);
    RUN_TEST(test_flush_rejects_uninitialized_context);
    RUN_TEST(test_flush_handles_wrapped_ring_buffer);

    RUN_TEST(test_status_reports_capacity_used_bytes_and_dropped);
    RUN_TEST(test_status_rejects_null_context);
    RUN_TEST(test_status_rejects_null_status);
    RUN_TEST(test_status_rejects_uninitialized_context);

    RUN_TEST(test_deinit_zeroes_context);
    RUN_TEST(test_deinit_rejects_null_context);
    RUN_TEST(test_deinit_rejects_uninitialized_context);

    RUN_TEST(test_context_uses_byte_buffer);

    return UNITY_END();
}

// EOF /////////////////////////////////////////////////////////////////////////////
