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

// @file xuart_fake.h
// @brief Host-test fake port for the xUART driver core.
//

#ifndef XUART_FAKE_H
#define XUART_FAKE_H

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
#include "xuart_driver.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

#define xUART_FAKE_RX_FILL_BYTE 0x00U

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct
    {
        // Injected return values for each op
        xRETURN_t next_init_status;
        xRETURN_t next_deinit_status;
        xRETURN_t next_start_status;
        xRETURN_t next_stop_status;
        xRETURN_t next_transmit_status;
        xRETURN_t next_receive_status;
        xRETURN_t next_transmit_async_status;
        xRETURN_t next_receive_async_status;
        xRETURN_t next_abort_tx_status;
        xRETURN_t next_abort_rx_status;

        // Async event injection: set to trigger a simulated completion event
        xUART_Driver_Event_Callback_t event_callback;
        void                         *event_callback_ctx;

        // Call counters
        uint32_t init_count;
        uint32_t deinit_count;
        uint32_t start_count;
        uint32_t stop_count;
        uint32_t transmit_count;
        uint32_t receive_count;
        uint32_t transmit_async_count;
        uint32_t receive_async_count;
        uint32_t abort_tx_count;
        uint32_t abort_rx_count;

        // Last transfer parameters
        const uint8_t *last_tx_buffer;
        uint8_t       *last_rx_buffer;
        uint32_t       last_tx_length;
        uint32_t       last_rx_length;

        // Port-side state
        bool is_initialized;
        bool is_started;
        bool is_tx_busy;
        bool is_rx_busy;
    } xUART_Fake_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    extern const xUART_Driver_Ops_t xUART_Fake_Driver_Ops;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

    void xUART_Fake_Context_Init(xUART_Fake_Context_t *fake_ctx);

    // Trigger a simulated async completion from test code (fires event_callback).
    void xUART_Fake_Fire_Tx_Complete(xUART_Fake_Context_t *fake_ctx, uint32_t bytes_transferred);
    void xUART_Fake_Fire_Rx_Complete(xUART_Fake_Context_t *fake_ctx, uint32_t bytes_transferred);
    void xUART_Fake_Fire_Event(xUART_Fake_Context_t *fake_ctx, xUART_Event_t event, const xUART_Event_Info_t *event_info);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUART_FAKE_H
// EOF /////////////////////////////////////////////////////////////////////////////
