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

// @file xbridge_qspi.h
// @brief xBRIDGE QSPI channel - WINUSB binary frame Quad-SPI bridge (SPI/Dual/Quad io_width).
//

#ifndef XBRIDGE_QSPI_H
#define XBRIDGE_QSPI_H

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

    // QSPI command opcodes (WINUSB binary frame cmd field)
#define xBRIDGE_QSPI_CMD_TRANSFER  0x01U
#define xBRIDGE_QSPI_CMD_SET_SPEED 0x02U
#define xBRIDGE_QSPI_CMD_SET_MODE  0x03U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Hardware QSPI peripheral ops table supplied by the port layer.
    typedef struct xBRIDGE_QSPI_Peripheral_Ops_t
    {
        xRETURN_t (*set_speed)(void *qspi_ctx, uint32_t hz);

        xRETURN_t (*transfer)(void *qspi_ctx,
                              uint8_t io_width, // 1, 2, or 4
                              uint8_t dummy_cycles,
                              uint32_t cmd,
                              uint32_t addr,
                              const uint8_t *tx_data,
                              uint8_t *rx_data,
                              uint32_t len);

    } xBRIDGE_QSPI_Peripheral_Ops_t;

    // QSPI bridge channel runtime context (caller-owned, zero-init before xBRIDGE_QSPI_Init).
    typedef struct xBRIDGE_QSPI_Context_t
    {
        const xBRIDGE_USB_Ops_t *usb_ops;
        void *usb_ctx;
        const xBRIDGE_QSPI_Peripheral_Ops_t *qspi_ops;
        void *qspi_ctx;

        uint8_t cmd_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Cmd_t)];
        uint8_t resp_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Resp_t)];
        xBRIDGE_State_t state;

    } xBRIDGE_QSPI_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the QSPI bridge channel context and wire both ops tables.
    xRETURN_t xBRIDGE_QSPI_Init(xBRIDGE_QSPI_Context_t *ctx,
                                const xBRIDGE_USB_Ops_t *usb_ops,
                                void *usb_ctx,
                                const xBRIDGE_QSPI_Peripheral_Ops_t *qspi_ops,
                                void *qspi_ctx);

    // Called from xUSBD WINUSB on_data_received for this channel's Bulk OUT endpoint.
    xRETURN_t xBRIDGE_QSPI_On_USB_Receive(xBRIDGE_QSPI_Context_t *ctx, const uint8_t *data, uint32_t length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_QSPI_H
// EOF /////////////////////////////////////////////////////////////////////////////
