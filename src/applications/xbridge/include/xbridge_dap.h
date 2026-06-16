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

// @file xbridge_dap.h
// @brief xBRIDGE CMSIS-DAP channel - ARM debug probe over HID v1 and WINUSB v2.
//

#ifndef XBRIDGE_DAP_H
#define XBRIDGE_DAP_H

#ifdef __cplusplus
extern "C"
{
#endif

    // INCLUDES ////////////////////////////////////////////////////////////////////
    // COMPILER INCLUDES
#include <stdint.h>

    // SYSTEM INCLUDES

    // MODULE INCLUDES
#include "xbridge_core.h"
#include "xbridge_defs.h"
#include "xbridge_return.h"
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // CMSIS-DAP command IDs (ARM specification)
#define xBRIDGE_DAP_ID_INFO               0x00U
#define xBRIDGE_DAP_ID_HOST_STATUS        0x01U
#define xBRIDGE_DAP_ID_CONNECT            0x02U
#define xBRIDGE_DAP_ID_DISCONNECT         0x03U
#define xBRIDGE_DAP_ID_TRANSFER_CONFIGURE 0x04U
#define xBRIDGE_DAP_ID_TRANSFER           0x05U
#define xBRIDGE_DAP_ID_TRANSFER_BLOCK     0x06U
#define xBRIDGE_DAP_ID_TRANSFER_ABORT     0x07U
#define xBRIDGE_DAP_ID_WRITE_ABORT        0x08U
#define xBRIDGE_DAP_ID_DELAY              0x09U
#define xBRIDGE_DAP_ID_RESET_TARGET       0x0AU
#define xBRIDGE_DAP_ID_SWJ_PINS           0x10U
#define xBRIDGE_DAP_ID_SWJ_CLOCK          0x11U
#define xBRIDGE_DAP_ID_SWJ_SEQUENCE       0x12U
#define xBRIDGE_DAP_ID_SWD_CONFIGURE      0x13U
#define xBRIDGE_DAP_ID_SWD_SEQUENCE       0x14U
#define xBRIDGE_DAP_ID_JTAG_SEQUENCE      0x15U
#define xBRIDGE_DAP_ID_JTAG_CONFIGURE     0x16U
#define xBRIDGE_DAP_ID_JTAG_IDCODE        0x17U

    // DAP connect mode values
#define xBRIDGE_DAP_CONNECT_NONE 0U
#define xBRIDGE_DAP_CONNECT_SWD  1U
#define xBRIDGE_DAP_CONNECT_JTAG 2U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Hardware debug probe peripheral ops table supplied by the port layer.
    typedef struct xBRIDGE_DAP_Peripheral_Ops_t
    {
        // Pin-level access (SWD/JTAG signal control)
        xRETURN_t (*pin_write)(void *dap_ctx, uint8_t pin_select, uint8_t pin_data);
        uint8_t (*pin_read)(void *dap_ctx, uint8_t pin_select);

        // SWJ bit-bang
        xRETURN_t (*swj_clock)(void *dap_ctx, uint32_t hz);
        xRETURN_t (*swj_sequence)(void *dap_ctx, uint32_t count, const uint8_t *data);
        xRETURN_t (*swd_sequence)(void *dap_ctx, uint32_t count, const uint8_t *in, uint8_t *out);

        // JTAG
        xRETURN_t (*jtag_sequence)(void *dap_ctx, uint32_t count, uint8_t tms_val, const uint8_t *tdi, uint8_t *tdo);

        // Target reset and microsecond delay
        xRETURN_t (*reset_target)(void *dap_ctx);
        xRETURN_t (*delay_us)(void *dap_ctx, uint32_t us);

    } xBRIDGE_DAP_Peripheral_Ops_t;

    // CMSIS-DAP bridge channel runtime context (caller-owned, zero-init before xBRIDGE_DAP_Init).
    typedef struct xBRIDGE_DAP_Context_t
    {
        const xBRIDGE_USB_Ops_t *usb_ops;
        void *usb_ctx;
        const xBRIDGE_DAP_Peripheral_Ops_t *dap_ops;
        void *dap_ctx;

        uint8_t req_buf[xBRIDGE_DAP_PACKET_BYTES];
        uint8_t resp_buf[xBRIDGE_DAP_PACKET_BYTES];

        // DAP protocol state
        uint8_t connect_mode; // xBRIDGE_DAP_CONNECT_*
        uint32_t swj_clock_hz;
        uint8_t idle_cycles;
        uint16_t wait_retry;
        uint16_t match_retry;

        // JTAG chain configuration
        uint8_t jtag_count;
        uint8_t jtag_ir_len[8U];

        xBRIDGE_State_t state;

    } xBRIDGE_DAP_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the CMSIS-DAP bridge channel context and wire both ops tables.
    xRETURN_t xBRIDGE_DAP_Init(xBRIDGE_DAP_Context_t *ctx,
                               const xBRIDGE_USB_Ops_t *usb_ops,
                               void *usb_ctx,
                               const xBRIDGE_DAP_Peripheral_Ops_t *dap_ops,
                               void *dap_ctx);

    // Called from xUSBD HID (v1) or xUSBD WINUSB (v2) on_data_received.
    // Dispatches the DAP command, executes via ops table, and calls usb_ops->send with the response.
    xRETURN_t xBRIDGE_DAP_On_USB_Receive(xBRIDGE_DAP_Context_t *ctx, const uint8_t *data, uint32_t length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_DAP_H
// EOF /////////////////////////////////////////////////////////////////////////////
