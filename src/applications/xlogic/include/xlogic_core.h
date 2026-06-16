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

// @file xlogic_core.h
// @brief xLOGIC core state machine - Arm, Abort, Poll, and state accessors.

#ifndef XLOGIC_CORE_H
#define XLOGIC_CORE_H

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
#include "xlogic_defs.h"
#include "xlogic_return.h"
#include "xlogic_pru.h"
#include "xlogic_transport.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Capture configuration supplied to xLOGIC_Arm.
    // Filled from SUMP context fields or set directly by WINUSB commands.
    typedef struct xLOGIC_Config_t
    {
        uint32_t sample_rate;               // Hz; clamped to [MIN, MAX] at arm time
        uint32_t sample_count;              // total samples to capture (0 = stream)
        uint32_t channel_mask;              // active channel bitmask (bits 0-7)
        xLOGIC_Trigger_Mode_t trigger_mode; // NONE / LEVEL / EDGE
        uint32_t trigger_mask;              // channel bitmask for trigger pattern
        uint32_t trigger_value;             // target pattern under trigger_mask
        uint32_t trigger_channel;           // single channel for PRU-side trigger
        uint32_t trigger_edge;              // xLOGIC_TRIGGER_EDGE_RISING/FALLING

    } xLOGIC_Config_t;

    // Core context.  All sub-context pointers are caller-owned.
    typedef struct xLOGIC_Core_Context_t
    {
        xLOGIC_State_t state;

        xLOGIC_PRU_Context_t *pru_ctx;
        xLOGIC_Transport_Context_t *transport_ctx;

        // Capture configuration set at Arm time
        xLOGIC_Config_t config;

        // ARM-side sample staging buffer (caller-owned, filled by xLOGIC_PRU_Read_Samples)
        uint8_t *sample_buffer;
        uint32_t sample_buffer_bytes;
        uint32_t samples_captured; // bytes drained into sample_buffer

    } xLOGIC_Core_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the core context, binding it to the PRU and transport sub-contexts.
    // sample_buffer: caller-provided staging area for captured samples.
    // sample_buffer_bytes: capacity of sample_buffer; must be >= config.sample_count.
    xRETURN_t xLOGIC_Init(xLOGIC_Core_Context_t *core_ctx,
                          xLOGIC_PRU_Context_t *pru_ctx,
                          xLOGIC_Transport_Context_t *transport_ctx,
                          uint8_t *sample_buffer,
                          uint32_t sample_buffer_bytes);

    // Configure the PRU trigger and start a capture.
    // config: capture parameters.  Caller retains ownership; fields are copied.
    xRETURN_t xLOGIC_Arm(xLOGIC_Core_Context_t *core_ctx, const xLOGIC_Config_t *config);

    // Abort any in-progress capture and return to IDLE.
    // Safe to call from any state; halts the PRU and clears pending requests.
    xRETURN_t xLOGIC_Abort(xLOGIC_Core_Context_t *core_ctx);

    // Advance the core state machine.
    // Call from an RTOS task or superloop; non-blocking.
    // In ARMED: checks PRU STATUS; transitions to DONE when STATUS_DONE.
    // In DONE:  drains samples into staging buffer, sends via transport, returns to IDLE.
    // In ERROR: halts PRU; application must call xLOGIC_Abort before re-arming.
    xRETURN_t xLOGIC_Poll(xLOGIC_Core_Context_t *core_ctx);

    // Return the current capture state.
    xLOGIC_State_t xLOGIC_Get_State(const xLOGIC_Core_Context_t *core_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XLOGIC_CORE_H
// EOF /////////////////////////////////////////////////////////////////////////////
