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

// @file xuart_drv.h
// @brief CH32H417 USART hardware port header for the xUART driver core.
//

#ifndef XUART_DRV_H
#define XUART_DRV_H

#ifdef __cplusplus
extern "C"
{
#endif

// INCLUDES ////////////////////////////////////////////////////////////////////////
// COMPILER INCLUDES
#include <stdbool.h>
#include <stdint.h>

// SYSTEM INCLUDES
#ifndef asm
#define asm __asm__
#endif
#include "ch32h417.h"

// MODULE INCLUDES
#include "xuart_driver.h"

    // MACROS //////////////////////////////////////////////////////////////////////////

    // TYPES ///////////////////////////////////////////////////////////////////////////

    typedef struct
    {
        USART_TypeDef                *usart;
        uint32_t                      pclk_hz;

        xUART_Driver_Event_Callback_t event_callback;
        void                         *event_callback_ctx;

        bool                          is_initialized;
        bool                          is_started;
        bool                          is_tx_busy;
        bool                          is_rx_busy;
        xRETURN_t                     last_tx_error;
        xRETURN_t                     last_rx_error;

        const uint8_t                *tx_buffer;
        uint32_t                      tx_length;
        uint32_t                      tx_index;

        uint8_t                      *rx_buffer;
        uint32_t                      rx_length;
        uint32_t                      rx_index;
    } xUART_CH32H417_Context_t;

    // VARIABLES ///////////////////////////////////////////////////////////////////////

    extern const xUART_Driver_Ops_t xUART_CH32H417_Driver_Ops;

    // INLINE FUNCTIONS ////////////////////////////////////////////////////////////////

    // FUNCTION PROTOTYPES /////////////////////////////////////////////////////////////

    // ISR entry points - call from the relevant USARTx_IRQHandler in board code.
    void xUART_CH32H417_IRQ_Handler(xUART_CH32H417_Context_t *ctx);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // XUART_DRV_H
// EOF /////////////////////////////////////////////////////////////////////////////
