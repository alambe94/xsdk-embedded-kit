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

// @file xbridge_can.h
// @brief xBRIDGE CAN channel - SLCAN over CDC ACM (Phase 6) and gs_usb binary (Phase 10).
//

#ifndef XBRIDGE_CAN_H
#define XBRIDGE_CAN_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xbridge_core.h"
#include "xbridge_config.h"
#include "xbridge_defs.h"
#include "xbridge_return.h"
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////

#define xBRIDGE_CAN_SLCAN_LINE_MAX  (32U) // maximum bytes in one SLCAN line
#define xBRIDGE_CAN_FRAME_ENTRY_LEN (14U) // id(4)+flags(1)+dlc(1)+data(8)

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Hardware CAN peripheral ops table supplied by the port layer.
    typedef struct xBRIDGE_CAN_Peripheral_Ops_t
    {
        xRETURN_t (*set_bitrate)(void *can_ctx, uint32_t bitrate_hz);
        xRETURN_t (*open)(void *can_ctx);
        xRETURN_t (*close)(void *can_ctx);

        xRETURN_t (*transmit)(void *can_ctx, uint32_t id, bool is_extended, bool is_rtr, uint8_t dlc, const uint8_t *data);

        bool (*rx_available)(void *can_ctx);

        xRETURN_t (*receive)(void *can_ctx, uint32_t *id, bool *is_extended, bool *is_rtr, uint8_t *dlc, uint8_t *data);

    } xBRIDGE_CAN_Peripheral_Ops_t;

    // CAN bridge channel runtime context (caller-owned, zero-init before xBRIDGE_CAN_Init).
    typedef struct xBRIDGE_CAN_Context_t
    {
        const xBRIDGE_USB_Ops_t *usb_ops;
        void *usb_ctx;
        const xBRIDGE_CAN_Peripheral_Ops_t *can_ops;
        void *can_ctx;

        // SLCAN line accumulator
        uint8_t slcan_line[xBRIDGE_CAN_SLCAN_LINE_MAX];
        uint32_t slcan_len;
        bool is_open;
        bool is_timestamp_enabled;

        xBRIDGE_State_t state;

        // RX frame ring for buffering received CAN frames before USB TX
        uint8_t rx_ring[xBRIDGE_CAN_QUEUE_FRAMES * xBRIDGE_CAN_FRAME_ENTRY_LEN];
        uint32_t rx_head;
        uint32_t rx_tail;

    } xBRIDGE_CAN_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the CAN bridge channel context and wire both ops tables.
    xRETURN_t xBRIDGE_CAN_Init(xBRIDGE_CAN_Context_t *ctx,
                               const xBRIDGE_USB_Ops_t *usb_ops,
                               void *usb_ctx,
                               const xBRIDGE_CAN_Peripheral_Ops_t *can_ops,
                               void *can_ctx);

    // Called from xUSBD CDC on_data_received - feeds SLCAN text bytes from the host.
    xRETURN_t xBRIDGE_CAN_On_USB_Receive(xBRIDGE_CAN_Context_t *ctx, const uint8_t *data, uint32_t length);

    // Called from the xRTOS CAN task - drains the RX frame ring to USB TX.
    xRETURN_t xBRIDGE_CAN_Poll(xBRIDGE_CAN_Context_t *ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_CAN_H
// EOF /////////////////////////////////////////////////////////////////////////////
