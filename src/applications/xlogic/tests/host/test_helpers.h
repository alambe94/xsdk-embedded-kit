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

// @file test_helpers.h
// @brief Shared test fixtures for xLOGIC host tests - fake MSRAM and fake TX sink.

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "xlogic_defs.h"
#include "xlogic_return.h"

// FAKE MSRAM //////////////////////////////////////////////////////////////////////

// 4 KB fake MSRAM: enough for the 64-byte control block and up to ~4000 test samples.
#define FAKE_MSRAM_BYTES 4096U

static uint8_t s_fake_msram[FAKE_MSRAM_BYTES];

// Typed accessor to the control block at offset 0
static inline xLOGIC_PRU_Control_t *fake_ctrl(void)
{
    return (xLOGIC_PRU_Control_t *)(&s_fake_msram[xLOGIC_PRU_CTRL_OFFSET]);
}

// Pointer to the sample data region (after the 64-byte control block)
static inline uint8_t *fake_ring(void)
{
    return (uint8_t *)(&s_fake_msram[xLOGIC_SAMPLE_DATA_OFFSET]);
}

// Reset the fake MSRAM to zero and put the PRU in READY state
static inline void reset_fake_msram(void)
{
    (void)memset(s_fake_msram, 0, sizeof(s_fake_msram));
    fake_ctrl()->status = xLOGIC_PRU_STATUS_READY;
    fake_ctrl()->cmd = xLOGIC_PRU_CMD_IDLE;
}

// Write count raw sample bytes into the fake ring and mark the PRU as done.
// Simulates a completed fixed-depth PRU capture.
static inline void inject_samples(const uint8_t *samples, uint32_t count)
{
    uint32_t available = FAKE_MSRAM_BYTES - xLOGIC_SAMPLE_DATA_OFFSET;
    uint32_t to_write = (count < available) ? count : available;

    (void)memcpy(fake_ring(), samples, to_write);
    fake_ctrl()->samples_captured = to_write;
    fake_ctrl()->status = xLOGIC_PRU_STATUS_DONE;
}

// Write count bytes of a repeating pattern byte into the fake ring and mark done.
static inline void inject_pattern(uint8_t pattern, uint32_t count)
{
    uint32_t available = FAKE_MSRAM_BYTES - xLOGIC_SAMPLE_DATA_OFFSET;
    uint32_t to_write = (count < available) ? count : available;

    (void)memset(fake_ring(), (int)pattern, to_write);
    fake_ctrl()->samples_captured = to_write;
    fake_ctrl()->status = xLOGIC_PRU_STATUS_DONE;
}

// Set the PRU status without touching sample data (for partial-capture tests).
static inline void set_pru_status(uint32_t status)
{
    fake_ctrl()->status = status;
}

// FAKE TX SINK ////////////////////////////////////////////////////////////////////

#define FAKE_TX_BUF_BYTES 8192U

static uint8_t s_tx_capture[FAKE_TX_BUF_BYTES];
static uint32_t s_tx_len = 0U;

// TX function injected into the transport context during tests.
// Appends all sent bytes to s_tx_capture for later inspection.
static inline xRETURN_t fake_tx_fn(void *ctx, const uint8_t *data, uint32_t length)
{
    (void)ctx;

    if ((s_tx_len + length) <= FAKE_TX_BUF_BYTES)
    {
        (void)memcpy(s_tx_capture + s_tx_len, data, length);
        s_tx_len += length;
    }

    return xRETURN_OK;
}

// Reset the TX capture buffer
static inline void reset_tx_capture(void)
{
    s_tx_len = 0U;
    (void)memset(s_tx_capture, 0, sizeof(s_tx_capture));
}

// SAMPLE BUFFER ///////////////////////////////////////////////////////////////////

// Staging buffer for core tests - sized to hold test sample sets comfortably
#define TEST_SAMPLE_BUF_BYTES 4000U

static uint8_t s_sample_buf[TEST_SAMPLE_BUF_BYTES];

static inline void reset_sample_buf(void)
{
    (void)memset(s_sample_buf, 0, sizeof(s_sample_buf));
}

#endif // TEST_HELPERS_H
// EOF /////////////////////////////////////////////////////////////////////////////
