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

// @file xlogic_defs.h
// @brief xLOGIC shared constants, MSRAM layout, PRU IPC structure, and capture enums.

#ifndef XLOGIC_DEFS_H
#define XLOGIC_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>
#include <stdbool.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES

    // MACROS //////////////////////////////////////////////////////////////////////

    // MSRAM layout (AM243x physical addresses, ARM view)
#define xLOGIC_MSRAM_BASE         0x70000000U
#define xLOGIC_MSRAM_BYTES        0x00200000U // 2 MB
#define xLOGIC_PRU_CTRL_OFFSET    0x00000000U // IPC control block at MSRAM start
#define xLOGIC_SAMPLE_DATA_OFFSET 0x00000040U // sample data starts after 64-byte ctrl
#define xLOGIC_MAX_SAMPLE_BYTES   (xLOGIC_MSRAM_BYTES - xLOGIC_SAMPLE_DATA_OFFSET)

    // Sample rate limits
#define xLOGIC_MAX_SAMPLE_RATE 333000000U // 333 MHz - PRU0 + RTU0 dual-core max
#define xLOGIC_MIN_SAMPLE_RATE 100000U    // 100 kHz
#define xLOGIC_SUMP_CLOCK_HZ   100000000U // 100 MHz SUMP reference clock

    // Channel configuration
#define xLOGIC_MAX_CHANNELS 8U // Phase 1: 8 channels via ICSSG0 PRU_GPI0-7

    // PRU command codes (ARM writes to control.cmd)
#define xLOGIC_PRU_CMD_IDLE 0x00000000U // PRU waits in ready loop
#define xLOGIC_PRU_CMD_ARM  0x00000001U // PRU begins capture
#define xLOGIC_PRU_CMD_HALT 0x00000002U // graceful stop at next buffer boundary

    // PRU status codes (PRU writes to control.status)
#define xLOGIC_PRU_STATUS_READY   0x00000000U // idle, ready to receive ARM command
#define xLOGIC_PRU_STATUS_RUNNING 0x00000001U // actively sampling
#define xLOGIC_PRU_STATUS_DONE    0x00000002U // sample_count reached (fixed-depth mode)
#define xLOGIC_PRU_STATUS_OVERRUN 0x00000003U // ARM did not drain in time

    // Trigger edge constants (stored in control.trigger_edge)
#define xLOGIC_TRIGGER_EDGE_RISING  0x00000000U
#define xLOGIC_TRIGGER_EDGE_FALLING 0x00000001U

    // SUMP protocol ID string (4 bytes, sent in response to 0x02 ID query)
#define xLOGIC_SUMP_ID_LEN 4U

    // TYPES ///////////////////////////////////////////////////////////////////////

    typedef enum xLOGIC_State_t
    {
        xLOGIC_STATE_IDLE = 0U,      // not armed; no capture in progress
        xLOGIC_STATE_ARMED = 1U,     // PRU running; waiting for PRU done
        xLOGIC_STATE_TRIGGERED = 2U, // trigger matched (reserved for streaming Phase 4)
        xLOGIC_STATE_CAPTURING = 3U, // post-trigger samples accumulating (Phase 4)
        xLOGIC_STATE_DONE = 4U,      // PRU done; samples ready to send to host
        xLOGIC_STATE_ERROR = 5U,     // PRU overrun or other unrecoverable error
    } xLOGIC_State_t;

    typedef enum xLOGIC_Trigger_Mode_t
    {
        xLOGIC_TRIGGER_NONE = 0U,  // start immediately; no trigger condition
        xLOGIC_TRIGGER_LEVEL = 1U, // trigger when (sample & mask) == value
        xLOGIC_TRIGGER_EDGE = 2U,  // trigger on rising or falling edge of a channel
    } xLOGIC_Trigger_Mode_t;

    // PRU IPC control block: occupies the first 64 bytes of MSRAM.
    // ARM writes cmd; PRU writes status, samples_captured, active_buf, overrun_count.
    // All accesses must go through a volatile pointer to this struct.
    typedef struct xLOGIC_PRU_Control_t
    {
        uint32_t cmd;              // xLOGIC_PRU_CMD_* - ARM writes, PRU reads
        uint32_t status;           // xLOGIC_PRU_STATUS_* - PRU writes, ARM reads
        uint32_t trigger_channel;  // 0-7: which channel triggers capture
        uint32_t trigger_edge;     // 0=rising, 1=falling
        uint32_t sample_count;     // 0=stream forever; nonzero=stop at N samples
        uint32_t samples_captured; // updated by PRU after each store burst
        uint32_t active_buf;       // 0=ping, 1=pong (streaming Phase 4 only)
        uint32_t overrun_count;    // incremented when ARM is too slow to drain
        uint32_t reserved[8U];     // padding to 64 bytes total
    } xLOGIC_PRU_Control_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XLOGIC_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
