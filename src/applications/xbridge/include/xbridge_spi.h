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

// @file xbridge_spi.h
// @brief xBRIDGE SPI channel - WINUSB binary frame full-duplex SPI controller bridge.
//

#ifndef XBRIDGE_SPI_H
#define XBRIDGE_SPI_H

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

    // SPI command opcodes (WINUSB binary frame cmd field)
#define xBRIDGE_SPI_CMD_TRANSFER    0x01U
#define xBRIDGE_SPI_CMD_CS_ASSERT   0x02U
#define xBRIDGE_SPI_CMD_CS_DEASSERT 0x03U
#define xBRIDGE_SPI_CMD_SET_MODE    0x04U
#define xBRIDGE_SPI_CMD_SET_SPEED   0x05U

    // SPI transfer flags field bits
#define xBRIDGE_SPI_FLAG_KEEP_CS (1U << 0U) // keep CS asserted after transfer

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Hardware SPI peripheral ops table supplied by the port layer.
    typedef struct xBRIDGE_SPI_Peripheral_Ops_t
    {
        xRETURN_t (*set_mode)(void *spi_ctx, uint8_t cpol, uint8_t cpha);
        xRETURN_t (*set_speed)(void *spi_ctx, uint32_t hz);
        xRETURN_t (*cs_assert)(void *spi_ctx, uint8_t cs_idx);
        xRETURN_t (*cs_deassert)(void *spi_ctx, uint8_t cs_idx);

        xRETURN_t (*transfer)(void *spi_ctx, const uint8_t *mosi, uint8_t *miso, uint32_t len);

    } xBRIDGE_SPI_Peripheral_Ops_t;

    // SPI bridge channel runtime context (caller-owned, zero-init before xBRIDGE_SPI_Init).
    typedef struct xBRIDGE_SPI_Context_t
    {
        const xBRIDGE_USB_Ops_t *usb_ops;
        void *usb_ctx;
        const xBRIDGE_SPI_Peripheral_Ops_t *spi_ops;
        void *spi_ctx;

        uint8_t cmd_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Cmd_t)];
        uint8_t resp_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Resp_t)];
        xBRIDGE_State_t state;

    } xBRIDGE_SPI_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the SPI bridge channel context and wire both ops tables.
    xRETURN_t xBRIDGE_SPI_Init(xBRIDGE_SPI_Context_t *ctx,
                               const xBRIDGE_USB_Ops_t *usb_ops,
                               void *usb_ctx,
                               const xBRIDGE_SPI_Peripheral_Ops_t *spi_ops,
                               void *spi_ctx);

    // Called from xUSBD WINUSB on_data_received for this channel's Bulk OUT endpoint.
    xRETURN_t xBRIDGE_SPI_On_USB_Receive(xBRIDGE_SPI_Context_t *ctx, const uint8_t *data, uint32_t length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_SPI_H
// EOF /////////////////////////////////////////////////////////////////////////////
