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

// @file xusbh_drv.h
// @brief Fake USB Host Controller Driver port for host-side verification.

#ifndef XUSBH_DRV_H
#define XUSBH_DRV_H

// INCLUDES ////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xusbh_hcd.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // MACROS //////////////////////////////////////////////////////////////////////
#define xUSBH_FAKE_HCD_MAX_PORTS            1U
#define xUSBH_FAKE_HCD_TRANSFER_QUEUE_DEPTH 8U
#define xUSBH_FAKE_HCD_DEFAULT_FRAME_NUMBER 0U
#define xUSBH_FAKE_HCD_DEFAULT_PORT         0U

    // TYPES ///////////////////////////////////////////////////////////////////////
    typedef struct xUSBH_Fake_HCD_Context_t
    {
        void *host_ctx;
        xUSBH_HCD_Event_Callback_t event_callback;
        xUSBH_HCD_Port_Status_t port_status;
        xUSBH_Transfer_t *transfer_queue[xUSBH_FAKE_HCD_TRANSFER_QUEUE_DEPTH];

        uint32_t transfer_read_idx;
        uint32_t transfer_write_idx;
        uint32_t transfer_count;
        uint32_t frame_number;

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

        xUSBH_Transfer_t *last_transfer;
        uint8_t last_port;
        bool last_port_power_enable;

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

        bool is_initialized;
        bool is_started;
        bool are_interrupts_enabled;
        bool is_port_powered;
    } xUSBH_Fake_HCD_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////
    extern const xUSBH_HCD_Ops_t xUSBH_Fake_HCD_Ops;
    extern xUSBH_Fake_HCD_Context_t xUSBH_Fake_HCD_Context;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////
    xRETURN_t xUSBH_Fake_HCD_Init(xUSBH_Fake_HCD_Context_t *fake_ctx);
    xRETURN_t xUSBH_Fake_HCD_Fire_Port_Event(xUSBH_Fake_HCD_Context_t *fake_ctx, uint8_t port, xUSBH_HCD_Port_Event_t port_event);
    xRETURN_t xUSBH_Fake_HCD_Complete_Transfer(xUSBH_Fake_HCD_Context_t *fake_ctx,
                                               xUSBH_Transfer_t *transfer,
                                               xUSBH_HCD_Transfer_Event_t transfer_event,
                                               uint32_t actual_length);
    xRETURN_t xUSBH_Fake_HCD_Submitted_Pop(xUSBH_Fake_HCD_Context_t *fake_ctx, xUSBH_Transfer_t **transfer);

#ifdef __cplusplus
}
#endif

#endif // XUSBH_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
