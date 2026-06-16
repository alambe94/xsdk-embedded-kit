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

// @file xbridge_uart.h
// @brief xBRIDGE UART channel - transparent CDC ACM to hardware UART bridge.
//

#ifndef XBRIDGE_UART_H
#define XBRIDGE_UART_H

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

    // TYPES ///////////////////////////////////////////////////////////////////////

    // Hardware UART peripheral ops table supplied by the port layer.
    typedef struct xBRIDGE_UART_Peripheral_Ops_t
    {
        // Apply CDC SET_LINE_CODING parameters to the hardware UART.
        xRETURN_t (*set_line_coding)(void *uart_ctx, uint32_t baud_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits);

        // Write bytes to the hardware UART TX.
        xRETURN_t (*write)(void *uart_ctx, const uint8_t *data, uint32_t length);

        // Read available bytes from the hardware UART RX into data.
        xRETURN_t (*read)(void *uart_ctx, uint8_t *data, uint32_t max_len, uint32_t *read_len);

        // Return true when UART RX data is available.
        bool (*is_rx_ready)(void *uart_ctx);

    } xBRIDGE_UART_Peripheral_Ops_t;

    // UART bridge channel runtime context (caller-owned, zero-init before xBRIDGE_UART_Init).
    typedef struct xBRIDGE_UART_Context_t
    {
        const xBRIDGE_USB_Ops_t *usb_ops;
        void *usb_ctx;
        const xBRIDGE_UART_Peripheral_Ops_t *uart_ops;
        void *uart_ctx;

        // Ring buffers for USB->UART and UART->USB byte flows.
        uint8_t usb_to_uart_buf[xBRIDGE_UART_QUEUE_BYTES];
        uint32_t usb_to_uart_head;
        uint32_t usb_to_uart_tail;

        uint8_t uart_to_usb_buf[xBRIDGE_UART_QUEUE_BYTES];
        uint32_t uart_to_usb_head;
        uint32_t uart_to_usb_tail;

        xBRIDGE_State_t state;
        bool is_dtr_active; // data terminal ready signal from host

    } xBRIDGE_UART_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////

    // Initialize the UART bridge channel context and wire both ops tables.
    xRETURN_t xBRIDGE_UART_Init(xBRIDGE_UART_Context_t *ctx,
                                const xBRIDGE_USB_Ops_t *usb_ops,
                                void *usb_ctx,
                                const xBRIDGE_UART_Peripheral_Ops_t *uart_ops,
                                void *uart_ctx);

    // Called from xUSBD CDC on_data_received - enqueues bytes for UART TX.
    xRETURN_t xBRIDGE_UART_On_USB_Receive(xBRIDGE_UART_Context_t *ctx, const uint8_t *data, uint32_t length);

    // Called from xUSBD CDC on_line_coding_set - forwards baud/stop/parity to UART.
    xRETURN_t
    xBRIDGE_UART_On_Line_Coding(xBRIDGE_UART_Context_t *ctx, uint32_t baud_rate, uint8_t stop_bits, uint8_t parity, uint8_t data_bits);

    // Called from xUSBD CDC on_control_line_state - tracks DTR/RTS signals.
    xRETURN_t xBRIDGE_UART_On_Control_Line_State(xBRIDGE_UART_Context_t *ctx, bool dtr, bool rts);

    // Called from the xRTOS UART task - drains UART RX ring to USB TX.
    xRETURN_t xBRIDGE_UART_Poll(xBRIDGE_UART_Context_t *ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XBRIDGE_UART_H
// EOF /////////////////////////////////////////////////////////////////////////////
