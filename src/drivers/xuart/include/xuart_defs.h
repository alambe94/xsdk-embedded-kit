// Copyright 2026 alambe94
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

// @file xuart_defs.h
// @brief Public xUART data types shared by the core, ports, and callers.
//

#ifndef XUART_DEFS_H
#define XUART_DEFS_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES

// MODULE INCLUDES
#include "xuart_return.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct xUART_Context_t xUART_Context_t;
    typedef struct xUART_Driver_Ops_t xUART_Driver_Ops_t;

    typedef enum
    {
        xUART_DATA_BITS_5 = 5U,
        xUART_DATA_BITS_6 = 6U,
        xUART_DATA_BITS_7 = 7U,
        xUART_DATA_BITS_8 = 8U,
        xUART_DATA_BITS_9 = 9U,
    } xUART_Data_Bits_t;

    typedef enum
    {
        xUART_STOP_BITS_1 = 0U,
        xUART_STOP_BITS_0_5 = 1U,
        xUART_STOP_BITS_2 = 2U,
        xUART_STOP_BITS_1_5 = 3U,
    } xUART_Stop_Bits_t;

    typedef enum
    {
        xUART_PARITY_NONE = 0U,
        xUART_PARITY_EVEN = 1U,
        xUART_PARITY_ODD = 2U,
    } xUART_Parity_t;

    typedef enum
    {
        xUART_FLOW_CONTROL_NONE = 0U,
        xUART_FLOW_CONTROL_RTS_CTS = 1U,
    } xUART_Flow_Control_t;

    typedef enum
    {
        xUART_EVENT_TX_COMPLETE = 0U,
        xUART_EVENT_TX_ABORTED = 1U,
        xUART_EVENT_TX_TIMEOUT = 2U,
        xUART_EVENT_RX_COMPLETE = 3U,
        xUART_EVENT_RX_ABORTED = 4U,
        xUART_EVENT_RX_TIMEOUT = 5U,
        xUART_EVENT_RX_OVERRUN = 6U,
        xUART_EVENT_RX_FRAMING = 7U,
        xUART_EVENT_RX_PARITY = 8U,
    } xUART_Event_t;

    typedef struct
    {
        uint32_t bytes_transferred;
        xRETURN_t error_code;
    } xUART_Event_Info_t;

    typedef struct
    {
        void (*on_event)(xUART_Context_t *uart_ctx, xUART_Event_t event, const xUART_Event_Info_t *event_info);
    } xUART_Callbacks_t;

    typedef struct
    {
        uint32_t baud_rate;
        xUART_Data_Bits_t data_bits;
        xUART_Stop_Bits_t stop_bits;
        xUART_Parity_t parity;
        xUART_Flow_Control_t flow_control;
        xUART_Callbacks_t callbacks;
    } xUART_Config_t;

    typedef struct
    {
        uint8_t port;
        const xUART_Driver_Ops_t *drv_ops;
        void *drv_ctx;
    } xUART_Start_Config_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUART_DEFS_H
// EOF /////////////////////////////////////////////////////////////////////////////
