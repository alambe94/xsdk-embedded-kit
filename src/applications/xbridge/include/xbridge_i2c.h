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

// @file xbridge_i2c.h
// @brief xBRIDGE I2C channel - WINUSB binary frame I2C controller bridge.
//

#ifndef XBRIDGE_I2C_H
#define XBRIDGE_I2C_H

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
#include "xbridge_defs.h"
#include "xbridge_return.h"
#include "xreturn.h"

    // MACROS //////////////////////////////////////////////////////////////////////

    // I2C command opcodes (WINUSB binary frame cmd field)
#define xBRIDGE_I2C_CMD_WRITE      0x01U
#define xBRIDGE_I2C_CMD_READ       0x02U
#define xBRIDGE_I2C_CMD_WRITE_READ 0x03U
#define xBRIDGE_I2C_CMD_SCAN       0x04U
#define xBRIDGE_I2C_CMD_SET_SPEED  0x05U

    // I2C flags field bits (in command payload)
#define xBRIDGE_I2C_FLAG_10BIT_ADDR (1U << 0U) // use 10-bit addressing
#define xBRIDGE_I2C_FLAG_NO_STOP    (1U << 1U) // suppress STOP for repeated START

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Hardware I2C peripheral ops table supplied by the port layer.
    typedef struct xBRIDGE_I2C_Peripheral_Ops_t
    {
        xRETURN_t (*set_speed)(void *i2c_ctx, uint32_t hz);

        xRETURN_t (*write)(void *i2c_ctx, uint16_t addr, const uint8_t *data, uint32_t len, bool no_stop);

        xRETURN_t (*read)(void *i2c_ctx, uint16_t addr, uint8_t *data, uint32_t len);

        xRETURN_t (*write_read)(void *i2c_ctx, uint16_t addr, const uint8_t *wdata, uint32_t wlen, uint8_t *rdata, uint32_t rlen);

    } xBRIDGE_I2C_Peripheral_Ops_t;

    // I2C bridge channel runtime context (caller-owned, zero-init before xBRIDGE_I2C_Init).
    typedef struct xBRIDGE_I2C_Context_t
    {
        const xBRIDGE_USB_Ops_t *usb_ops;
        void *usb_ctx;
        const xBRIDGE_I2C_Peripheral_Ops_t *i2c_ops;
        void *i2c_ctx;

        uint8_t cmd_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Cmd_t)];
        uint8_t resp_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Resp_t)];
        xBRIDGE_State_t state;

    } xBRIDGE_I2C_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the I2C bridge channel context and wire both ops tables.
    xRETURN_t xBRIDGE_I2C_Init(xBRIDGE_I2C_Context_t *ctx,
                               const xBRIDGE_USB_Ops_t *usb_ops,
                               void *usb_ctx,
                               const xBRIDGE_I2C_Peripheral_Ops_t *i2c_ops,
                               void *i2c_ctx);

    // Called from xUSBD WINUSB on_data_received for this channel's Bulk OUT endpoint.
    xRETURN_t xBRIDGE_I2C_On_USB_Receive(xBRIDGE_I2C_Context_t *ctx, const uint8_t *data, uint32_t length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_I2C_H
// EOF /////////////////////////////////////////////////////////////////////////////
