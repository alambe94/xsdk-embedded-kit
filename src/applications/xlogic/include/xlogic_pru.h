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

// @file xlogic_pru.h
// @brief xLOGIC PRU IPC layer - control block accessor, ARM/HALT, and sample drain.

#ifndef XLOGIC_PRU_H
#define XLOGIC_PRU_H

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

    // MACROS //////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Runtime context for the PRU IPC layer.
    // All fields are set by xLOGIC_PRU_Init and must not be modified directly.
    typedef struct xLOGIC_PRU_Context_t
    {
        volatile xLOGIC_PRU_Control_t *control; // IPC control block in MSRAM
        volatile uint8_t *ring;                 // sample data region start
        uint32_t ring_bytes;                    // capacity of sample data region
        uint32_t read_offset;                   // bytes drained so far this capture
        bool is_loaded;                         // PRU firmware has been loaded

    } xLOGIC_PRU_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the PRU context, pointing it at the MSRAM base address.
    // msram_base: physical (or test) base address of the MSRAM region.
    // msram_bytes: total size of the MSRAM region in bytes.
    // On hardware pass (void *)xLOGIC_MSRAM_BASE; for host tests pass a static array.
    xRETURN_t xLOGIC_PRU_Init(xLOGIC_PRU_Context_t *pru_ctx, void *msram_base, uint32_t msram_bytes);

    // Load the PRU firmware binary into ICSSG IRAM and start the PRU cores.
    // On the host test port this is a no-op that sets is_loaded = true.
    xRETURN_t xLOGIC_PRU_Load_Firmware(xLOGIC_PRU_Context_t *pru_ctx);

    // Configure the trigger and write CMD_ARM to start a capture.
    // trigger_channel: 0-7 (ignored when trigger_mode == xLOGIC_TRIGGER_NONE).
    // trigger_edge: xLOGIC_TRIGGER_EDGE_RISING or xLOGIC_TRIGGER_EDGE_FALLING.
    // sample_count: number of samples to capture (0 = stream forever).
    xRETURN_t xLOGIC_PRU_Arm(xLOGIC_PRU_Context_t *pru_ctx, uint32_t trigger_channel, uint32_t trigger_edge, uint32_t sample_count);

    // Request the PRU to stop at the next buffer boundary (write CMD_HALT).
    xRETURN_t xLOGIC_PRU_Halt(xLOGIC_PRU_Context_t *pru_ctx);

    // Copy available samples from the PRU ring into dest.
    // Copies up to max_bytes or whatever the PRU has written since the last drain.
    // bytes_read_out: number of bytes actually copied.
    xRETURN_t xLOGIC_PRU_Read_Samples(xLOGIC_PRU_Context_t *pru_ctx, uint8_t *dest, uint32_t max_bytes, uint32_t *bytes_read_out);

    // Return the number of bytes available to drain without blocking.
    uint32_t xLOGIC_PRU_Available_Bytes(const xLOGIC_PRU_Context_t *pru_ctx);

    // Return true if the PRU has set STATUS_OVERRUN since the last Arm call.
    bool xLOGIC_PRU_Has_Overrun(const xLOGIC_PRU_Context_t *pru_ctx);

    // Return the current PRU status word.
    uint32_t xLOGIC_PRU_Get_Status(const xLOGIC_PRU_Context_t *pru_ctx);

    // Reset the read offset (called at the start of each new capture).
    void xLOGIC_PRU_Reset_Read_Offset(xLOGIC_PRU_Context_t *pru_ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XLOGIC_PRU_H
// EOF /////////////////////////////////////////////////////////////////////////////
