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

// @file xbridge_gpio.h
// @brief xBRIDGE GPIO channel - WINUSB binary frame pin direction, read, write, and pull control.
//

#ifndef XBRIDGE_GPIO_H
#define XBRIDGE_GPIO_H

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

    // GPIO command opcodes (WINUSB binary frame cmd field)
#define xBRIDGE_GPIO_CMD_SET_DIRECTION 0x01U // configure pin(s) as input or output
#define xBRIDGE_GPIO_CMD_WRITE         0x02U // set output level for one or more pins
#define xBRIDGE_GPIO_CMD_READ          0x03U // read input level for one or more pins
#define xBRIDGE_GPIO_CMD_SET_PULL      0x04U // configure pull-up / pull-down / floating
#define xBRIDGE_GPIO_CMD_SET_MODE      0x05U // set alternate function, open-drain, etc.
#define xBRIDGE_GPIO_CMD_TOGGLE        0x06U // toggle output level for one or more pins

    // GPIO direction values (used in SET_DIRECTION payload)
#define xBRIDGE_GPIO_DIR_INPUT  0x00U
#define xBRIDGE_GPIO_DIR_OUTPUT 0x01U

    // GPIO pull configuration values (used in SET_PULL payload)
#define xBRIDGE_GPIO_PULL_NONE 0x00U
#define xBRIDGE_GPIO_PULL_UP   0x01U
#define xBRIDGE_GPIO_PULL_DOWN 0x02U

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Hardware GPIO peripheral ops table supplied by the port layer.
    typedef struct xBRIDGE_GPIO_Peripheral_Ops_t
    {
        // Configure a single pin as input or output.
        xRETURN_t (*set_direction)(void *gpio_ctx, uint32_t port, uint32_t pin, uint8_t direction);

        // Write a value (0 or 1) to a single output pin.
        xRETURN_t (*write_pin)(void *gpio_ctx, uint32_t port, uint32_t pin, uint8_t value);

        // Write a masked value to an entire port (value & mask applied).
        xRETURN_t (*write_port)(void *gpio_ctx, uint32_t port, uint32_t value, uint32_t mask);

        // Read the current level of a single pin (returns 0 or 1).
        xRETURN_t (*read_pin)(void *gpio_ctx, uint32_t port, uint32_t pin, uint8_t *value);

        // Read all pins of a port at once.
        xRETURN_t (*read_port)(void *gpio_ctx, uint32_t port, uint32_t *value);

        // Configure pull-up / pull-down / floating on a single pin.
        xRETURN_t (*set_pull)(void *gpio_ctx, uint32_t port, uint32_t pin, uint8_t pull);

        // Toggle the output level of a single pin.
        xRETURN_t (*toggle_pin)(void *gpio_ctx, uint32_t port, uint32_t pin);

    } xBRIDGE_GPIO_Peripheral_Ops_t;

    // GPIO bridge channel runtime context (caller-owned, zero-init before xBRIDGE_GPIO_Init).
    typedef struct xBRIDGE_GPIO_Context_t
    {
        const xBRIDGE_USB_Ops_t *usb_ops;
        void *usb_ctx;
        const xBRIDGE_GPIO_Peripheral_Ops_t *gpio_ops;
        void *gpio_ctx;

        uint8_t cmd_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Cmd_t)];
        uint8_t resp_buf[xBRIDGE_MAX_PAYLOAD_BYTES + sizeof(xBRIDGE_Frame_Resp_t)];
        xBRIDGE_State_t state;

    } xBRIDGE_GPIO_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the GPIO bridge channel context and wire both ops tables.
    xRETURN_t xBRIDGE_GPIO_Init(xBRIDGE_GPIO_Context_t *ctx,
                                const xBRIDGE_USB_Ops_t *usb_ops,
                                void *usb_ctx,
                                const xBRIDGE_GPIO_Peripheral_Ops_t *gpio_ops,
                                void *gpio_ctx);

    // Called from xUSBD WINUSB on_data_received for this channel's Bulk OUT endpoint.
    xRETURN_t xBRIDGE_GPIO_On_USB_Receive(xBRIDGE_GPIO_Context_t *ctx, const uint8_t *data, uint32_t length);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_GPIO_H
// EOF /////////////////////////////////////////////////////////////////////////////
